// Загружает общие настройки булева и ранжированного консольного поиска.
#include "boolean_search/app/config.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Завершает запуск поисковой утилиты при ошибке конфигурации.
[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

// Удаляет пробельные символы по краям изменяемой строки.
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

// Разбирает строго положительное целое значение без лишних символов.
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

}  // namespace

// Читает пути индекса, словарей и ограничения выдачи из config.yaml.
BooleanSearchConfig load_boolean_search_config(const char *path) {
    BooleanSearchConfig config{};
    config.uri = "mongodb://localhost:27017";
    config.database = "mai_ir";
    config.processed_collection = "processed_documents";
    config.index_directory = "results/boolean_index";
    config.lemma_dictionary = "resources/wordnet_exceptions.tsv";
    config.lemma_vocabulary = "resources/wordnet_lemmas.txt";
    config.lemma_overrides = "resources/technical_lemma_overrides.tsv";
    config.maximum_token_length = 100;
    config.result_limit = 20;
    config.tfidf_result_limit = 20;

    // Используется только необходимый проекту поднабор простого YAML.
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        fail("не удалось открыть config.yaml");
    }
    char line[8192];
    std::string section;
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
            section = key;
            continue;
        }
        bool valid = true;
        if (section == "db") {
            if (std::strcmp(key, "uri") == 0) {
                config.uri = value;
            } else if (std::strcmp(key, "database") == 0) {
                config.database = value;
            } else if (std::strcmp(key, "processed_collection") == 0) {
                config.processed_collection = value;
            }
        } else if (section == "linguistics") {
            if (std::strcmp(key, "maximum_token_length") == 0) {
                valid = parse_positive_size(value, &config.maximum_token_length);
            } else if (std::strcmp(key, "lemma_dictionary") == 0) {
                config.lemma_dictionary = value;
            } else if (std::strcmp(key, "lemma_vocabulary") == 0) {
                config.lemma_vocabulary = value;
            } else if (std::strcmp(key, "lemma_overrides") == 0) {
                config.lemma_overrides = value;
            }
        } else if (section == "boolean_index" &&
                   std::strcmp(key, "output_directory") == 0) {
            config.index_directory = value;
        } else if (section == "boolean_search" &&
                   std::strcmp(key, "result_limit") == 0) {
            valid = parse_positive_size(value, &config.result_limit);
        } else if (section == "tfidf_search" &&
                   std::strcmp(key, "result_limit") == 0) {
            valid = parse_positive_size(value, &config.tfidf_result_limit);
        }
        if (!valid) {
            std::fclose(file);
            fail("некорректное значение в config.yaml");
        }
    }
    std::fclose(file);
    return config;
}
