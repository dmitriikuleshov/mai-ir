#include "tfidf_search/app/console.hpp"

#include "boolean_search/index/index_reader.hpp"
#include "tfidf_search/ranking/search.hpp"

#include <bson/bson.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

void trim_string(std::string *value) {
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
        if (*cursor == '\n' || *cursor == '\r' || *cursor == '\t') {
            std::putchar(' ');
        } else {
            std::putchar(*cursor);
        }
    }
}

void print_document(mongoc_collection_t *collection,
                    const uint8_t oid_bytes[12],
                    const RankedDocument &ranked, size_t position) {
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
        std::printf("%zu. ", position);
        print_clean(utf8_field(document, "title"));
        std::printf("\n   score=%.6f | источник: ", ranked.score);
        print_clean(utf8_field(document, "source"));
        std::printf(" | doc=%u | oid=%s\n   ", ranked.document_id, oid_text);
        print_clean(utf8_field(document, "url"));
        std::putchar('\n');
    } else {
        std::printf("%zu. score=%.6f | doc=%u | oid=%s | метаданные не найдены\n",
                    position, ranked.score, ranked.document_id, oid_text);
    }
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
}

}  // namespace

void run_tfidf_search_console(const BooleanSearchConfig &config,
                              TfidfIndex *index,
                              const LemmaDictionary *dictionary,
                              mongoc_collection_t *collection) {
    std::string query;
    const bool interactive = isatty(STDIN_FILENO) != 0;
    if (interactive) {
        std::fprintf(stderr,
                     "Введите запрос на естественном языке. Ctrl+D — выход.\n> ");
    }
    while (std::getline(std::cin, query)) {
        trim_string(&query);
        if (query.empty()) {
            if (interactive) std::fprintf(stderr, "> ");
            continue;
        }

        char error[512] = "";
        const auto started = std::chrono::steady_clock::now();
        const TfidfSearchResult result = tfidf_search(
            index, query.c_str(), config.maximum_token_length,
            config.tfidf_result_limit, dictionary, error, sizeof(error));
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started);

        std::printf("Запрос: %s\n", query.c_str());
        if (error[0] != '\0') {
            std::printf("Ошибка запроса: %s\n\n", error);
        } else {
            std::printf("Совпало документов: %zu | показано: %zu | поиск: %.3f мс\n",
                        result.matched_documents, result.documents.size(),
                        elapsed.count());
            for (size_t position = 0; position < result.documents.size();
                 ++position) {
                const RankedDocument &ranked = result.documents[position];
                uint8_t oid[12];
                if (!boolean_index_document_oid(
                        &index->boolean_index, ranked.document_id, oid)) {
                    std::printf("%zu. score=%.6f | doc=%u | ошибка таблицы документов\n",
                                position + 1, ranked.score,
                                ranked.document_id);
                    continue;
                }
                print_document(collection, oid, ranked, position + 1);
            }
            std::putchar('\n');
        }
        std::fflush(stdout);
        if (interactive) std::fprintf(stderr, "> ");
    }
}
