// Повторно нормализует текст, сохраняет позиции лемм и запускает сортировку.
#include "positional_index/building/builder.hpp"

#include "boolean_index/storage/files.hpp"
#include "boolean_index/storage/format.hpp"
#include "linguistics/language/pipeline.hpp"
#include "positional_index/building/blocks.hpp"
#include "positional_index/storage/writer.hpp"

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {

[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

FILE *open_document_table(const PositionalIndexConfig &config,
                          boolean_index_format::DocumentHeader *header) {
    char path[PATH_SIZE];
    join_path(path, sizeof(path), config.boolean_index_directory.c_str(),
              "documents.bin");
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr ||
        std::fread(header, sizeof(*header), 1, file) != 1 ||
        std::memcmp(header->magic, boolean_index_format::DOCUMENT_MAGIC,
                    sizeof(header->magic)) != 0 ||
        header->version != boolean_index_format::VERSION) {
        if (file != nullptr) std::fclose(file);
        fail("сначала необходимо построить совместимый булев индекс");
    }
    return file;
}

const char *document_text(const bson_t *document, uint32_t *length) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, "text") ||
        !BSON_ITER_HOLDS_UTF8(&iterator)) {
        return nullptr;
    }
    return bson_iter_utf8(&iterator, length);
}

}  // namespace

void build_positional_index(const PositionalIndexConfig &config) {
    make_directories(config.output_directory.c_str());
    char temporary_template[PATH_SIZE];
    join_path(temporary_template, sizeof(temporary_template),
              config.output_directory.c_str(), "blocks-XXXXXX");
    if (mkdtemp(temporary_template) == nullptr) {
        fail("не удалось создать временный каталог координат");
    }

    boolean_index_format::DocumentHeader document_header{};
    FILE *documents = open_document_table(config, &document_header);
    LemmaDictionary *dictionary = lemma_dictionary_load(
        config.lemma_dictionary.c_str(), config.lemma_vocabulary.c_str(),
        config.lemma_overrides.c_str());
    if (dictionary == nullptr) fail("не удалось загрузить словарь лемматизации");

    mongoc_client_t *client = mongoc_client_new(config.uri.c_str());
    if (client == nullptr) fail("не удалось создать клиент MongoDB");
    mongoc_collection_t *collection = mongoc_client_get_collection(
        client, config.database.c_str(), config.processed_collection.c_str());

    bson_t filter = BSON_INITIALIZER;
    BSON_APPEND_BOOL(&filter, "accepted", true);
    bson_t options = BSON_INITIALIZER;
    bson_t projection;
    BSON_APPEND_DOCUMENT_BEGIN(&options, "projection", &projection);
    BSON_APPEND_INT32(&projection, "text", 1);
    bson_append_document_end(&options, &projection);
    bson_t sort;
    BSON_APPEND_DOCUMENT_BEGIN(&options, "sort", &sort);
    BSON_APPEND_INT32(&sort, "_id", 1);
    bson_append_document_end(&options, &sort);
    BSON_APPEND_INT32(&options, "batchSize", 20);

    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, &filter, &options, nullptr);
    PositionBuffer buffer;
    PositionBlockList blocks;
    uint32_t document_id = 0;
    const bson_t *document = nullptr;
    while (mongoc_cursor_next(cursor, &document)) {
        if (document_id >= document_header.document_count) {
            fail("обработанный корпус содержит документы вне булева индекса");
        }
        bson_iter_t id_iterator;
        if (!bson_iter_init_find(&id_iterator, document, "_id") ||
            !BSON_ITER_HOLDS_OID(&id_iterator)) {
            fail("обработанный документ не содержит ObjectId");
        }
        uint8_t expected_oid[12];
        if (std::fread(expected_oid, 1, sizeof(expected_oid), documents) !=
                sizeof(expected_oid) ||
            std::memcmp(expected_oid, bson_iter_oid(&id_iterator)->bytes,
                        sizeof(expected_oid)) != 0) {
            fail("documents.bin не соответствует processed_documents; перестройте индексы");
        }

        uint32_t text_length = 0;
        const char *text = document_text(document, &text_length);
        if (text == nullptr) fail("принятый документ не содержит извлечённый текст");
        TokenList lemmas = linguistics_normalize_sequence(
            text, text_length, config.maximum_token_length, dictionary);
        if (lemmas.size() > UINT32_MAX) {
            fail("число токенов документа превышает формат координат");
        }
        for (size_t position = 0; position < lemmas.size(); ++position) {
            position_buffer_add(&buffer, lemmas[position], document_id,
                                static_cast<uint32_t>(position));
        }
        ++document_id;
        if (buffer.size() >= config.block_occurrences) {
            flush_position_block(&buffer, &blocks, temporary_template);
        }
        if (document_id % config.progress_every_documents == 0) {
            std::fprintf(stderr,
                         "Координатный индекс: обработано документов %u\n",
                         document_id);
        }
    }
    bson_error_t error{};
    if (mongoc_cursor_error(cursor, &error)) {
        std::fprintf(stderr, "Ошибка MongoDB: %s\n", error.message);
        std::exit(EXIT_FAILURE);
    }
    if (document_id != document_header.document_count) {
        fail("число processed_documents не совпадает с documents.bin");
    }

    flush_position_block(&buffer, &blocks, temporary_template);
    reduce_position_blocks(&blocks, temporary_template);
    merge_position_blocks(&blocks, config.output_directory.c_str(),
                          document_id);

    for (const std::string &path : blocks) unlink(path.c_str());
    rmdir(temporary_template);
    std::fclose(documents);
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    lemma_dictionary_destroy(dictionary);
}
