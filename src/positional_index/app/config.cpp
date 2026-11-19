// Читает параметры MongoDB, лингвистики и внешней сортировки координат.
#include "positional_index/app/config.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

char *trim(char *value) {
    while (*value == ' ' || *value == '\t') ++value;
    char *end = value + std::strlen(value);
    while (end > value &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
            end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return value;
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

void resolve_path(std::string *value, const std::string &config_path) {
    if (value->empty() || value->front() == '/') return;
    const size_t separator = config_path.find_last_of('/');
    if (separator != std::string::npos) {
        *value = config_path.substr(0, separator + 1) + *value;
    }
}

}  // namespace

PositionalIndexConfig load_positional_index_config(const char *path) {
    PositionalIndexConfig config{};
    config.uri = "mongodb://localhost:27017";
    config.database = "mai_ir";
    config.processed_collection = "processed_documents";
    config.output_directory = "results/positional_index";
    config.boolean_index_directory = "results/boolean_index";
    config.lemma_dictionary = "resources/wordnet_exceptions.tsv";
    config.lemma_vocabulary = "resources/wordnet_lemmas.txt";
    config.lemma_overrides = "resources/technical_lemma_overrides.tsv";
    config.maximum_token_length = 100;
    config.block_occurrences = 500000;
    config.progress_every_documents = 1000;

    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) fail("не удалось открыть конфигурацию координатного индекса");
    char line[8192];
    std::string section;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char *comment = std::strchr(line, '#');
        if (comment != nullptr) *comment = '\0';
        const size_t indentation = std::strspn(line, " \t");
        char *content = trim(line);
        if (*content == '\0' || *content == '-') continue;
        char *colon = std::strchr(content, ':');
        if (colon == nullptr) continue;
        *colon = '\0';
        char *key = trim(content);
        char *value = trim(colon + 1);
        if (indentation == 0 && *value == '\0') {
            section = key;
            continue;
        }
        bool valid = true;
        if (section == "db") {
            if (std::strcmp(key, "uri") == 0) config.uri = value;
            else if (std::strcmp(key, "database") == 0) config.database = value;
            else if (std::strcmp(key, "processed_collection") == 0) {
                config.processed_collection = value;
            }
        } else if (section == "linguistics") {
            if (std::strcmp(key, "maximum_token_length") == 0) {
                valid = parse_positive_size(value,
                                            &config.maximum_token_length);
            } else if (std::strcmp(key, "lemma_dictionary") == 0) {
                config.lemma_dictionary = value;
            } else if (std::strcmp(key, "lemma_vocabulary") == 0) {
                config.lemma_vocabulary = value;
            } else if (std::strcmp(key, "lemma_overrides") == 0) {
                config.lemma_overrides = value;
            }
        } else if (section == "positional_index") {
            if (std::strcmp(key, "output_directory") == 0) {
                config.output_directory = value;
            } else if (std::strcmp(key, "boolean_index_directory") == 0) {
                config.boolean_index_directory = value;
            } else if (std::strcmp(key, "block_occurrences") == 0) {
                valid = parse_positive_size(value, &config.block_occurrences);
            } else if (std::strcmp(key, "progress_every_documents") == 0) {
                valid = parse_positive_size(
                    value, &config.progress_every_documents);
            }
        }
        if (!valid) {
            std::fclose(file);
            fail("некорректное значение конфигурации координатного индекса");
        }
    }
    std::fclose(file);
    if (config.output_directory.empty() ||
        config.boolean_index_directory.empty()) {
        fail("не заданы каталоги координатного и булева индексов");
    }
    resolve_path(&config.output_directory, path);
    resolve_path(&config.boolean_index_directory, path);
    resolve_path(&config.lemma_dictionary, path);
    resolve_path(&config.lemma_vocabulary, path);
    resolve_path(&config.lemma_overrides, path);
    return config;
}
