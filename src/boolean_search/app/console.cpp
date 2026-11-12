// Реализует интерактивный ввод булевых запросов и человекочитаемый вывод результатов.
#include "boolean_search/app/console.hpp"

#include <bson/bson.h>

#include "boolean_search/index/index_reader.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

// Удаляет пробелы и переводы строк по краям введённого запроса.
void trim_string(std::string *value) {
    const size_t begin = value->find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        value->clear();
        return;
    }
    const size_t end = value->find_last_not_of(" \t\r\n");
    *value = value->substr(begin, end - begin + 1);
}

// Безопасно возвращает строковое поле BSON или пустую строку при его отсутствии.
const char *utf8_field(const bson_t *document, const char *field) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, field) ||
        !BSON_ITER_HOLDS_UTF8(&iterator)) {
        return "";
    }
    return bson_iter_utf8(&iterator, nullptr);
}

// Печатает строку в одну консольную строку, заменяя управляющие пробелы обычными.
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

// Находит метаданные документа по ObjectId и печатает один результат выдачи.
void print_document(mongoc_collection_t *collection,
                    const uint8_t oid_bytes[12], uint32_t internal_id,
                    size_t position) {
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
        std::printf("\n   Источник: ");
        print_clean(utf8_field(document, "source"));
        std::printf(" | doc=%u | oid=%s\n   ", internal_id, oid_text);
        print_clean(utf8_field(document, "url"));
        std::putchar('\n');
    } else {
        std::printf("%zu. doc=%u | oid=%s | метаданные не найдены\n",
                    position, internal_id, oid_text);
    }
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
}

}  // namespace

// Читает запросы до EOF, выполняет поиск и выводит ограниченное число документов.
void run_boolean_search_console(const BooleanSearchConfig &config,
                                BooleanIndex *index,
                                const LemmaDictionary *dictionary,
                                mongoc_collection_t *collection) {
    std::string query;
    // При перенаправлении stdin подсказки скрываются, поэтому вывод удобно проверять скриптом.
    const bool interactive = isatty(STDIN_FILENO) != 0;
    if (interactive) {
        std::fprintf(stderr,
                     "Введите запрос с AND, OR, NOT и скобками. Ctrl+D — выход.\n> ");
    }
    while (std::getline(std::cin, query)) {
        trim_string(&query);
        if (query.empty()) {
            if (interactive) {
                std::fprintf(stderr, "> ");
            }
            continue;
        }
        char error[512] = "";
        BooleanResult result = boolean_search(
            index, query.c_str(), config.maximum_token_length, dictionary,
            error, sizeof(error));
        std::printf("Запрос: %s\n", query.c_str());
        if (error[0] != '\0') {
            std::printf("Ошибка запроса: %s\n\n", error);
        } else {
            std::printf("Найдено документов: %zu\n", result.size());
            const size_t shown = result.size() < config.result_limit
                                     ? result.size()
                                     : config.result_limit;
            // Поиск возвращает docID, а documents.bin связывает его с ObjectId MongoDB.
            for (size_t position = 0; position < shown; ++position) {
                uint8_t oid[12];
                if (!boolean_index_document_oid(index, result[position], oid)) {
                    std::printf("%zu. doc=%u | ошибка таблицы документов\n",
                                position + 1, result[position]);
                    continue;
                }
                print_document(collection, oid, result[position], position + 1);
            }
            if (shown < result.size()) {
                std::printf("... показано %zu из %zu\n", shown, result.size());
            }
            std::putchar('\n');
        }
        std::fflush(stdout);
        if (interactive) {
            std::fprintf(stderr, "> ");
        }
    }
}
