#include "engine.hpp"

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

struct Config {
    char uri[1024];
    char database[256];
    char processed_collection[256];
    char index_directory[4096];
    char lemma_dictionary[4096];
    size_t maximum_token_length;
    size_t result_limit;
};

void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

char *trim(char *value) {
    while (*value == ' ' || *value == '\t') {
        ++value;
    }
    char *end = value + std::strlen(value);
    while (end > value &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
            end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return value;
}

bool copy_value(char *target, size_t capacity, const char *value) {
    if (std::strlen(value) >= capacity) {
        return false;
    }
    std::snprintf(target, capacity, "%s", value);
    return true;
}

bool parse_positive_size(const char *value, size_t *result) {
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno != 0 || end == value || *trim(end) != '\0' || parsed == 0 ||
        parsed > SIZE_MAX) {
        return false;
    }
    *result = static_cast<size_t>(parsed);
    return true;
}

Config load_config(const char *path) {
    Config config{};
    copy_value(config.uri, sizeof(config.uri), "mongodb://localhost:27017");
    copy_value(config.database, sizeof(config.database), "mai_ir");
    copy_value(config.processed_collection,
               sizeof(config.processed_collection), "processed_documents");
    copy_value(config.index_directory, sizeof(config.index_directory),
               "results/boolean_index");
    copy_value(config.lemma_dictionary, sizeof(config.lemma_dictionary),
               "resources/wordnet_exceptions.tsv");
    config.maximum_token_length = 100;
    config.result_limit = 20;

    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        fail("не удалось открыть config.yaml");
    }
    char line[8192];
    char section[128] = "";
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char *comment = std::strchr(line, '#');
        if (comment != nullptr) {
            *comment = '\0';
        }
        const size_t indentation = std::strspn(line, " \t");
        char *content = trim(line);
        if (*content == '\0' || *content == '-') {
            continue;
        }
        char *colon = std::strchr(content, ':');
        if (colon == nullptr) {
            continue;
        }
        *colon = '\0';
        char *key = trim(content);
        char *value = trim(colon + 1);
        if (indentation == 0 && *value == '\0') {
            if (!copy_value(section, sizeof(section), key)) {
                fail("слишком длинное имя раздела конфигурации");
            }
            continue;
        }
        bool valid = true;
        if (std::strcmp(section, "db") == 0) {
            if (std::strcmp(key, "uri") == 0) {
                valid = copy_value(config.uri, sizeof(config.uri), value);
            } else if (std::strcmp(key, "database") == 0) {
                valid = copy_value(config.database, sizeof(config.database), value);
            } else if (std::strcmp(key, "processed_collection") == 0) {
                valid = copy_value(config.processed_collection,
                                   sizeof(config.processed_collection), value);
            }
        } else if (std::strcmp(section, "linguistics") == 0) {
            if (std::strcmp(key, "maximum_token_length") == 0) {
                valid = parse_positive_size(value, &config.maximum_token_length);
            } else if (std::strcmp(key, "lemma_dictionary") == 0) {
                valid = copy_value(config.lemma_dictionary,
                                   sizeof(config.lemma_dictionary), value);
            }
        } else if (std::strcmp(section, "boolean_index") == 0 &&
                   std::strcmp(key, "output_directory") == 0) {
            valid = copy_value(config.index_directory,
                               sizeof(config.index_directory), value);
        } else if (std::strcmp(section, "boolean_search") == 0 &&
                   std::strcmp(key, "result_limit") == 0) {
            valid = parse_positive_size(value, &config.result_limit);
        }
        if (!valid) {
            std::fclose(file);
            fail("некорректное значение в config.yaml");
        }
    }
    std::fclose(file);
    return config;
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

void print_document(mongoc_collection_t *collection, const uint8_t oid_bytes[12],
                    uint32_t internal_id, size_t position) {
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

void run_queries(const Config *config, BooleanIndex *index,
                 const LemmaDictionary *dictionary,
                 mongoc_collection_t *collection) {
    char *line = nullptr;
    size_t capacity = 0;
    const bool interactive = isatty(STDIN_FILENO) != 0;
    if (interactive) {
        std::fprintf(stderr,
                     "Введите запрос с AND, OR, NOT и скобками. Ctrl+D — выход.\n> ");
    }
    while (getline(&line, &capacity, stdin) >= 0) {
        char *query = trim(line);
        if (*query == '\0') {
            if (interactive) {
                std::fprintf(stderr, "> ");
            }
            continue;
        }
        char error[512] = "";
        BooleanResult result = boolean_search(index, query,
                                              config->maximum_token_length,
                                              dictionary, error, sizeof(error));
        std::printf("Запрос: %s\n", query);
        if (error[0] != '\0') {
            std::printf("Ошибка запроса: %s\n\n", error);
        } else {
            std::printf("Найдено документов: %zu\n", result.size);
            const size_t shown = result.size < config->result_limit
                                     ? result.size
                                     : config->result_limit;
            for (size_t position = 0; position < shown; ++position) {
                uint8_t oid[12];
                if (!boolean_index_document_oid(
                        index, result.document_ids[position], oid)) {
                    std::printf("%zu. doc=%u | ошибка таблицы документов\n",
                                position + 1, result.document_ids[position]);
                    continue;
                }
                print_document(collection, oid, result.document_ids[position],
                               position + 1);
            }
            if (shown < result.size) {
                std::printf("... показано %zu из %zu\n", shown, result.size);
            }
            std::putchar('\n');
        }
        boolean_result_destroy(&result);
        std::fflush(stdout);
        if (interactive) {
            std::fprintf(stderr, "> ");
        }
    }
    std::free(line);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s config.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    const Config config = load_config(argv[1]);
    BooleanIndex index{};
    char error[512] = "";
    if (!boolean_index_open(&index, config.index_directory, error,
                            sizeof(error))) {
        fail(error);
    }
    LemmaDictionary *dictionary =
        lemma_dictionary_load(config.lemma_dictionary);
    if (dictionary == nullptr) {
        boolean_index_close(&index);
        fail("не удалось загрузить словарь лемматизации");
    }
    mongoc_init();
    mongoc_client_t *client = mongoc_client_new(config.uri);
    if (client == nullptr) {
        fail("не удалось создать клиент MongoDB");
    }
    mongoc_collection_t *collection = mongoc_client_get_collection(
        client, config.database, config.processed_collection);
    run_queries(&config, &index, dictionary, collection);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_cleanup();
    lemma_dictionary_destroy(dictionary);
    boolean_index_close(&index);
    return EXIT_SUCCESS;
}
