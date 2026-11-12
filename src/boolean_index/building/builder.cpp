// Координирует чтение терминов корпуса и построение всех файлов индекса.
#include "boolean_index/building/builder.hpp"

#include "boolean_index/storage/files.hpp"
#include "boolean_index/storage/format.hpp"
#include "boolean_index/storage/index_writer.hpp"
#include "tfidf_search/ranking/weights.hpp"

#include <mongoc/mongoc.h>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <unistd.h>

namespace {

// Немедленно завершает построение, чтобы не сохранить частично корректный индекс.
[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

}  // namespace

// Извлекает пары «терм — частота» документа, добавляет postings и вычисляет L2-норму.
bool add_document_terms(const bson_t *document, const char *field,
                        uint32_t document_id, PostingBuffer *buffer,
                        double *document_norm) {
    bson_iter_t field_iterator;
    if (!bson_iter_init_find(&field_iterator, document, field) ||
        !BSON_ITER_HOLDS_ARRAY(&field_iterator)) {
        return false;
    }
    bson_iter_t terms;
    if (!bson_iter_recurse(&field_iterator, &terms)) {
        return false;
    }
    double squared_norm = 0.0;
    while (bson_iter_next(&terms)) {
        // Каждый элемент MongoDB-массива имеет вид ["терм", частота].
        if (!BSON_ITER_HOLDS_ARRAY(&terms)) {
            return false;
        }
        bson_iter_t pair;
        if (!bson_iter_recurse(&terms, &pair) || !bson_iter_next(&pair) ||
            !BSON_ITER_HOLDS_UTF8(&pair)) {
            return false;
        }
        uint32_t length = 0;
        const char *term = bson_iter_utf8(&pair, &length);
        if (!bson_iter_next(&pair) ||
            (!BSON_ITER_HOLDS_INT32(&pair) &&
             !BSON_ITER_HOLDS_INT64(&pair))) {
            return false;
        }
        const int64_t stored_frequency = bson_iter_as_int64(&pair);
        if (stored_frequency <= 0 || stored_frequency > UINT32_MAX) {
            return false;
        }
        if (length != 0 &&
            !posting_buffer_add(buffer, term, length, document_id,
                                static_cast<uint32_t>(stored_frequency))) {
            fail("недостаточно памяти для блока индекса");
        }
        if (length != 0) {
            // Для схемы lnc используется вес 1 + log10(tf), без IDF документа.
            const double weight = logarithmic_term_frequency(
                static_cast<uint64_t>(stored_frequency));
            squared_norm += weight * weight;
        }
    }
    if (document_norm != nullptr) *document_norm = std::sqrt(squared_norm);
    return true;
}

// Строит индекс блоками, не загружая все пары большого корпуса в память одновременно.
void build_boolean_index(const BooleanIndexConfig &config) {
    make_directories(config.output_directory.c_str());
    char temporary_template[PATH_SIZE];
    join_path(temporary_template, sizeof(temporary_template),
              config.output_directory.c_str(), "blocks-XXXXXX");
    if (mkdtemp(temporary_template) == nullptr) {
        fail("не удалось создать временный каталог");
    }
    char documents_path[PATH_SIZE];
    join_path(documents_path, sizeof(documents_path),
              config.output_directory.c_str(), "documents.bin.tmp");
    FILE *documents = std::fopen(documents_path, "wb+");
    if (documents == nullptr) {
        fail("не удалось создать таблицу документов");
    }
    boolean_index_format::DocumentHeader empty_header{};
    // Место под заголовок резервируется заранее и заполняется после подсчёта документов.
    if (std::fwrite(&empty_header, sizeof(empty_header), 1, documents) != 1) {
        fail("не удалось записать таблицу документов");
    }
    char norms_path[PATH_SIZE];
    join_path(norms_path, sizeof(norms_path), config.output_directory.c_str(),
              "norms.bin.tmp");
    FILE *norms = std::fopen(norms_path, "wb+");
    if (norms == nullptr) {
        fail("не удалось создать таблицу норм документов");
    }
    boolean_index_format::DocumentNormHeader empty_norm_header{};
    if (std::fwrite(&empty_norm_header, sizeof(empty_norm_header), 1, norms) !=
        1) {
        fail("не удалось записать таблицу норм документов");
    }

    mongoc_client_t *client = mongoc_client_new(config.uri.c_str());
    if (client == nullptr) {
        fail("не удалось создать клиент MongoDB");
    }
    mongoc_collection_t *collection = mongoc_client_get_collection(
        client, config.database.c_str(), config.terms_collection.c_str());
    bson_t filter = BSON_INITIALIZER;
    bson_t options = BSON_INITIALIZER;
    bson_t projection;
    bson_t sort;
    BSON_APPEND_DOCUMENT_BEGIN(&options, "projection", &projection);
    BSON_APPEND_INT32(&projection, config.term_field.c_str(), 1);
    bson_append_document_end(&options, &projection);
    BSON_APPEND_DOCUMENT_BEGIN(&options, "sort", &sort);
    // Сортировка по ObjectId делает назначение последовательных docID воспроизводимым.
    BSON_APPEND_INT32(&sort, "_id", 1);
    bson_append_document_end(&options, &sort);
    BSON_APPEND_INT32(&options, "batchSize", 100);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, &filter, &options, nullptr);

    PostingBuffer buffer{};
    BlockList blocks{};
    uint32_t document_count = 0;
    const bson_t *document = nullptr;
    while (mongoc_cursor_next(cursor, &document)) {
        bson_iter_t id_iterator;
        if (!bson_iter_init_find(&id_iterator, document, "_id") ||
            !BSON_ITER_HOLDS_OID(&id_iterator)) {
            fail("document_terms содержит документ без ObjectId");
        }
        if (document_count == UINT32_MAX) {
            fail("число документов превышает формат индекса");
        }
        const bson_oid_t *oid = bson_iter_oid(&id_iterator);
        // Позиция ObjectId в documents.bin соответствует назначенному числовому docID.
        if (std::fwrite(oid->bytes, 1, sizeof(oid->bytes), documents) !=
            sizeof(oid->bytes)) {
            fail("ошибка записи идентификатора документа");
        }
        double document_norm = 0.0;
        if (!add_document_terms(document, config.term_field.c_str(),
                                document_count, &buffer, &document_norm)) {
            fail("некорректный массив терминов в document_terms");
        }
        if (std::fwrite(&document_norm, sizeof(document_norm), 1, norms) != 1) {
            fail("ошибка записи нормы документа");
        }
        ++document_count;
        // При достижении лимита буфер сортируется и освобождается в файл на диске.
        if (buffer.size() >= config.block_postings) {
            flush_block(&buffer, &blocks, temporary_template);
        }
        if (document_count % config.progress_every_documents == 0) {
            std::fprintf(stderr, "Индекс: прочитано документов %u\n",
                         document_count);
        }
    }
    bson_error_t error{};
    if (mongoc_cursor_error(cursor, &error)) {
        std::fprintf(stderr, "Ошибка MongoDB: %s\n", error.message);
        std::exit(EXIT_FAILURE);
    }
    flush_block(&buffer, &blocks, temporary_template);
    finalize_documents(documents, documents_path,
                       config.output_directory.c_str(), document_count);
    finalize_document_norms(norms, norms_path,
                            config.output_directory.c_str(), document_count);
    // Сначала ограничивается число файлов, затем выполняется единое финальное слияние.
    reduce_blocks(&blocks, temporary_template);
    merge_blocks(&blocks, config.output_directory.c_str(), document_count);

    for (const std::string &path : blocks) {
        unlink(path.c_str());
    }
    rmdir(temporary_template);
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
}
