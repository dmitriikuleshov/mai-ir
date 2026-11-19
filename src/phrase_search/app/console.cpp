// Читает по одной цитате из stdin и выводит документы и позиции вхождений.
#include "phrase_search/app/console.hpp"

#include "phrase_search/index/index_reader.hpp"
#include "phrase_search/query/search.hpp"

#include <bson/bson.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

void trim(std::string *value) {
    const size_t begin = value->find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        value->clear();
        return;
    }
    const size_t end = value->find_last_not_of(" \t\r\n");
    *value = value->substr(begin, end - begin + 1);
}

const char *utf8_field(const bson_t *document, const char *field) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, field) ||
        !BSON_ITER_HOLDS_UTF8(&iterator)) {
        return "";
    }
    return bson_iter_utf8(&iterator, nullptr);
}

void print_clean(const char *text) {
    for (const unsigned char *cursor =
             reinterpret_cast<const unsigned char *>(text);
         *cursor != '\0'; ++cursor) {
        std::putchar(*cursor == '\n' || *cursor == '\r' || *cursor == '\t'
                         ? ' '
                         : *cursor);
    }
}

void print_positions(const std::vector<uint32_t> &positions) {
    constexpr size_t MAXIMUM_SHOWN_POSITIONS = 10;
    std::printf("   Начальные позиции (с 0): ");
    const size_t shown = positions.size() < MAXIMUM_SHOWN_POSITIONS
                             ? positions.size()
                             : MAXIMUM_SHOWN_POSITIONS;
    for (size_t index = 0; index < shown; ++index) {
        if (index != 0) std::printf(", ");
        std::printf("%u", positions[index]);
    }
    if (shown < positions.size()) {
        std::printf(", ... (всего %zu)", positions.size());
    }
    std::putchar('\n');
}

void print_document(mongoc_collection_t *collection, PositionalIndex *index,
                    const PhraseMatch &match, size_t number) {
    uint8_t oid_bytes[12];
    if (!positional_index_document_oid(index, match.document_id, oid_bytes)) {
        std::printf("%zu. doc=%u | ошибка таблицы документов\n", number,
                    match.document_id);
        return;
    }
    bson_oid_t oid;
    bson_oid_init_from_data(&oid, oid_bytes);
    char oid_text[25];
    bson_oid_to_string(&oid, oid_text);
    bson_t filter = BSON_INITIALIZER;
    BSON_APPEND_OID(&filter, "_id", &oid);
    bson_t options = BSON_INITIALIZER;
    BSON_APPEND_INT64(&options, "limit", 1);
    bson_t projection;
    BSON_APPEND_DOCUMENT_BEGIN(&options, "projection", &projection);
    BSON_APPEND_INT32(&projection, "title", 1);
    BSON_APPEND_INT32(&projection, "url", 1);
    BSON_APPEND_INT32(&projection, "source", 1);
    bson_append_document_end(&options, &projection);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, &filter, &options, nullptr);
    const bson_t *document = nullptr;
    if (mongoc_cursor_next(cursor, &document)) {
        std::printf("%zu. ", number);
        print_clean(utf8_field(document, "title"));
        std::printf("\n   Источник: ");
        print_clean(utf8_field(document, "source"));
        std::printf(" | doc=%u | oid=%s\n   ", match.document_id, oid_text);
        print_clean(utf8_field(document, "url"));
        std::putchar('\n');
    } else {
        std::printf("%zu. doc=%u | oid=%s | метаданные не найдены\n", number,
                    match.document_id, oid_text);
    }
    print_positions(match.start_positions);
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
}

}  // namespace

void run_phrase_search_console(const BooleanSearchConfig &config,
                               PositionalIndex *index,
                               const LemmaDictionary *dictionary,
                               mongoc_collection_t *collection) {
    std::string query;
    const bool interactive = isatty(STDIN_FILENO) != 0;
    if (interactive) {
        std::fprintf(stderr,
                     "Введите точную цитату, например \"distributed database\". Ctrl+D - выход.\n> ");
    }
    while (std::getline(std::cin, query)) {
        trim(&query);
        if (query.empty()) {
            if (interactive) std::fprintf(stderr, "> ");
            continue;
        }
        char error[512] = "";
        const auto started = std::chrono::steady_clock::now();
        const PhraseSearchResult result = phrase_search(
            index, query.c_str(), config.maximum_token_length, dictionary,
            error, sizeof(error));
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started);
        std::printf("Цитата: %s\n", query.c_str());
        if (error[0] != '\0') {
            std::printf("Ошибка запроса: %s\n\n", error);
        } else {
            std::printf(
                "Найдено документов: %zu | вхождений: %llu | поиск: %.3f мс\n",
                result.documents.size(),
                static_cast<unsigned long long>(result.occurrence_count),
                elapsed.count());
            const size_t shown =
                result.documents.size() < config.phrase_result_limit
                    ? result.documents.size()
                    : config.phrase_result_limit;
            for (size_t index_value = 0; index_value < shown; ++index_value) {
                print_document(collection, index, result.documents[index_value],
                               index_value + 1);
            }
            if (shown < result.documents.size()) {
                std::printf("... показано %zu из %zu\n", shown,
                            result.documents.size());
            }
            std::putchar('\n');
        }
        std::fflush(stdout);
        if (interactive) std::fprintf(stderr, "> ");
    }
}
