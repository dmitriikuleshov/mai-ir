// Выполняет запросы веб-интерфейса существующими C++-алгоритмами и возвращает JSON.
#include "boolean_search/app/config.hpp"
#include "boolean_search/index/index_reader.hpp"
#include "boolean_search/query/query_parser.hpp"
#include "phrase_search/index/index_reader.hpp"
#include "phrase_search/query/search.hpp"
#include "tfidf_search/ranking/search.hpp"

#include <mongoc/mongoc.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Держит открытые индексы, словарь и соединение между последовательными запросами.
struct SearchResources {
    TfidfIndex index{};
    PositionalIndex positions{};
    bool positions_open = false;
    LemmaDictionary *dictionary = nullptr;
    mongoc_client_t *client = nullptr;
    mongoc_collection_t *collection = nullptr;

    ~SearchResources() {
        if (collection != nullptr) mongoc_collection_destroy(collection);
        if (client != nullptr) mongoc_client_destroy(client);
        if (dictionary != nullptr) lemma_dictionary_destroy(dictionary);
        positional_index_close(&positions);
        tfidf_index_close(&index);
    }
};

// Приводит результаты трёх алгоритмов к общему виду для страницы.
struct SearchHit {
    uint32_t document_id;
    double score = 0.0;
    std::vector<uint32_t> positions;
};

// Читает строковое поле JSON/BSON; отсутствующее поле даёт пустую строку.
const char *string_field(const bson_t *document, const char *name) {
    bson_iter_t field;
    return bson_iter_init_find(&field, document, name) &&
                   BSON_ITER_HOLDS_UTF8(&field)
               ? bson_iter_utf8(&field, nullptr) : "";
}

// Формирует короткое начало текста, не разрезая многобайтный UTF-8-символ.
std::string text_preview(const char *text) {
    size_t end = 0;
    size_t characters = 0;
    while (text[end] != '\0') {
        const unsigned char byte = static_cast<unsigned char>(text[end]);
        if ((byte & 0xC0U) != 0x80U && ++characters > 260) break;
        ++end;
    }
    std::string preview(text, end);
    if (text[end] != '\0') preview += "…";
    return preview;
}

// Передаёт одну JSON-строку; stdout используется только для протокола с сервером.
void send_response(const bson_t *response) {
    char *json = bson_as_relaxed_extended_json(response, nullptr);
    std::puts(json);
    std::fflush(stdout);
    bson_free(json);
}

// Возвращает понятную ошибку без смешивания её с обычными результатами.
void send_error(const char *message, const char *kind = "query") {
    bson_t response = BSON_INITIALIZER;
    BSON_APPEND_BOOL(&response, "ok", false);
    BSON_APPEND_UTF8(&response, "error", message);
    BSON_APPEND_UTF8(&response, "kind", kind);
    send_response(&response);
    bson_destroy(&response);
}

// Дополняет найденный docID заголовком, URL и коротким началом текста из MongoDB.
bool append_document(SearchResources *resources, const SearchHit &hit,
                     const char *mode, bson_t *output, std::string *error) {
    uint8_t bytes[12];
    if (!boolean_index_document_oid(&resources->index.boolean_index,
                                     hit.document_id, bytes)) {
        *error = "Не удалось прочитать таблицу документов индекса.";
        return false;
    }
    bson_oid_t oid;
    bson_oid_init_from_data(&oid, bytes);
    char oid_text[25];
    bson_oid_to_string(&oid, oid_text);
    BSON_APPEND_INT64(output, "doc_id", hit.document_id);
    BSON_APPEND_UTF8(output, "oid", oid_text);

    bson_t filter = BSON_INITIALIZER;
    BSON_APPEND_OID(&filter, "_id", &oid);
    bson_t options = BSON_INITIALIZER;
    BSON_APPEND_INT64(&options, "limit", 1);
    bson_t projection;
    BSON_APPEND_DOCUMENT_BEGIN(&options, "projection", &projection);
    for (const char *name : {"title", "url", "source", "text"}) {
        BSON_APPEND_INT32(&projection, name, 1);
    }
    bson_append_document_end(&options, &projection);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        resources->collection, &filter, &options, nullptr);
    const bson_t *document = nullptr;
    if (mongoc_cursor_next(cursor, &document)) {
        for (const char *name : {"title", "url", "source"}) {
            BSON_APPEND_UTF8(output, name, string_field(document, name));
        }
        const std::string preview = text_preview(string_field(document, "text"));
        BSON_APPEND_UTF8(output, "preview", preview.c_str());
    } else {
        bson_error_t database_error;
        if (mongoc_cursor_error(cursor, &database_error)) {
            *error = "MongoDB недоступна. Проверьте, что docker compose up -d выполнена.";
        } else {
            const std::string title = "Документ " + std::to_string(hit.document_id);
            BSON_APPEND_UTF8(output, "title", title.c_str());
            BSON_APPEND_BOOL(output, "metadata_missing", true);
        }
    }
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    if (!error->empty()) return false;

    if (std::string(mode) == "tfidf") BSON_APPEND_DOUBLE(output, "score", hit.score);
    if (std::string(mode) == "phrase") {
        BSON_APPEND_INT64(output, "occurrences", hit.positions.size());
        bson_t positions;
        BSON_APPEND_ARRAY_BEGIN(output, "positions", &positions);
        // В интерфейсе достаточно первых десяти позиций; общее число хранится отдельно.
        for (size_t i = 0; i < std::min<size_t>(hit.positions.size(), 10); ++i) {
            const std::string key = std::to_string(i);
            BSON_APPEND_INT64(&positions, key.c_str(), hit.positions[i]);
        }
        bson_append_array_end(output, &positions);
    }
    return true;
}

// Выполняет один запрос через готовые поисковые функции, не дублируя их алгоритмы.
void execute_query(const bson_t *request, const BooleanSearchConfig &config,
                   SearchResources *resources) {
    const std::string mode = string_field(request, "mode");
    if (mode == "status") {
        bson_t response = BSON_INITIALIZER;
        BSON_APPEND_BOOL(&response, "ok", true);
        BSON_APPEND_INT64(&response, "documents",
                          resources->index.boolean_index.header.document_count);
        send_response(&response);
        bson_destroy(&response);
        return;
    }
    const std::string query = string_field(request, "query");
    if (query.empty() || query.size() > 8000) {
        send_error("Введите запрос длиной до 1000 символов.");
        return;
    }
    char error[512] = "";
    // Координатный индекс открывается при первом цитатном запросе.
    if (mode == "phrase" && !resources->positions_open) {
        if (!positional_index_open(&resources->positions,
                                    config.positional_index_directory.c_str(),
                                    config.index_directory.c_str(), error, sizeof(error))) {
            send_error("Координатный индекс недоступен. Сначала запустите positional_index.",
                       "unavailable");
            return;
        }
        resources->positions_open = true;
    }
    const auto started = std::chrono::steady_clock::now();
    std::vector<SearchHit> hits;
    size_t total = 0;
    uint64_t occurrences = 0;
    if (mode == "boolean") {
        const BooleanResult result = boolean_search(
            &resources->index.boolean_index, query.c_str(),
            config.maximum_token_length, resources->dictionary,
            config.use_skip_pointers, nullptr, error, sizeof(error));
        total = result.size();
        for (size_t i = 0; i < std::min(total, config.result_limit); ++i) {
            hits.push_back({result[i], 0.0, {}});
        }
    } else if (mode == "tfidf") {
        const TfidfSearchResult result = tfidf_search(
            &resources->index, query.c_str(), config.maximum_token_length,
            config.tfidf_result_limit, resources->dictionary, error, sizeof(error));
        total = result.matched_documents;
        for (const RankedDocument &document : result.documents) {
            hits.push_back({document.document_id, document.score, {}});
        }
    } else if (mode == "phrase") {
        const PhraseSearchResult result = phrase_search(
            &resources->positions, query.c_str(), config.maximum_token_length,
            resources->dictionary, error, sizeof(error));
        total = result.documents.size();
        occurrences = result.occurrence_count;
        for (size_t i = 0; i < std::min(total, config.phrase_result_limit); ++i) {
            hits.push_back({result.documents[i].document_id, 0.0,
                            result.documents[i].start_positions});
        }
    } else {
        send_error("Неизвестный режим поиска.");
        return;
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    if (error[0] != '\0') {
        send_error(error);
        return;
    }
    bson_t response = BSON_INITIALIZER;
    BSON_APPEND_BOOL(&response, "ok", true);
    BSON_APPEND_UTF8(&response, "query", query.c_str());
    BSON_APPEND_UTF8(&response, "mode", mode.c_str());
    BSON_APPEND_INT64(&response, "total", total);
    BSON_APPEND_DOUBLE(&response, "search_ms", elapsed);
    if (mode == "phrase") BSON_APPEND_INT64(&response, "occurrences", occurrences);
    bson_t results;
    BSON_APPEND_ARRAY_BEGIN(&response, "results", &results);
    std::string metadata_error;
    for (size_t i = 0; i < hits.size(); ++i) {
        const std::string key = std::to_string(i);
        bson_t document;
        BSON_APPEND_DOCUMENT_BEGIN(&results, key.c_str(), &document);
        const bool success = append_document(resources, hits[i], mode.c_str(),
                                              &document, &metadata_error);
        bson_append_document_end(&results, &document);
        if (!success) break;
    }
    bson_append_array_end(&response, &results);
    if (metadata_error.empty()) send_response(&response);
    else send_error(metadata_error.c_str(), "unavailable");
    bson_destroy(&response);
}

// Загружает общие ресурсы и читает по одному JSON-запросу до закрытия stdin.
int run_worker(const char *config_path) {
    const BooleanSearchConfig config = load_boolean_search_config(config_path);
    SearchResources resources;
    char error[512] = "";
    if (!tfidf_index_open(&resources.index, config.index_directory.c_str(),
                          error, sizeof(error))) {
        std::fprintf(stderr, "Индекс недоступен: %s\n", error);
        return EXIT_FAILURE;
    }
    resources.dictionary = lemma_dictionary_load(
        config.lemma_dictionary.c_str(), config.lemma_vocabulary.c_str(),
        config.lemma_overrides.c_str());
    mongoc_uri_t *uri = mongoc_uri_new(config.uri.c_str());
    if (uri == nullptr) return EXIT_FAILURE;
    mongoc_uri_set_option_as_int32(uri, "serverSelectionTimeoutMS", 3000);
    resources.client = mongoc_client_new_from_uri(uri);
    mongoc_uri_destroy(uri);
    if (resources.client == nullptr) return EXIT_FAILURE;
    resources.collection = mongoc_client_get_collection(
        resources.client, config.database.c_str(), config.processed_collection.c_str());
    std::string line;
    while (std::getline(std::cin, line)) {
        bson_error_t parse_error;
        bson_t *request = bson_new_from_json(
            reinterpret_cast<const uint8_t *>(line.data()), line.size(), &parse_error);
        if (request == nullptr) send_error("Неверный JSON-запрос.");
        else {
            execute_query(request, config, &resources);
            bson_destroy(request);
        }
    }
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s configs/search.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    mongoc_init();
    const int status = run_worker(argv[1]);
    mongoc_cleanup();
    return status;
}
