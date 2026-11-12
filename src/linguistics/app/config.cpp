#include "linguistics/app/config.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

[[noreturn]] void fail(const char *message, const char *detail = nullptr) {
    std::fprintf(stderr, "linguistics: ");
    std::fprintf(stderr, message, detail);
    std::fputc('\n', stderr);
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
    if ((*value == '\'' || *value == '"') && end - value >= 2 &&
        end[-1] == *value) {
        ++value;
        end[-1] = '\0';
    }
    return value;
}

void resolve_path(std::string *value, const std::string &config_path) {
    if (value->empty() || value->front() == '/') return;
    const size_t separator = config_path.find_last_of('/');
    if (separator == std::string::npos) return;
    *value = config_path.substr(0, separator + 1) + *value;
}

uint64_t parse_positive(const char *value, const char *name) {
    char *end = nullptr;
    errno = 0;
    const unsigned long long result = std::strtoull(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || result == 0) {
        std::fprintf(stderr,
                     "linguistics: %s должен быть положительным "
                     "целым числом\n",
                     name);
        std::exit(EXIT_FAILURE);
    }
    return result;
}

Config default_config() {
    Config config;
    config.uri = "mongodb://localhost:27017";
    config.database = "mai_ir";
    config.documents = "documents";
    config.processed = "processed_documents";
    config.document_terms = "document_terms";
    config.token_frequencies = "token_frequencies";
    config.lemma_frequencies = "lemma_frequencies";
    config.state = "linguistics_state";
    config.lemma_dictionary = "resources/wordnet_exceptions.tsv";
    config.lemma_vocabulary = "resources/wordnet_lemmas.txt";
    config.lemma_overrides = "resources/technical_lemma_overrides.tsv";
    config.output_directory = "results";
    config.batch_size = 100;
    config.minimum_words = 500;
    config.maximum_token_length = 100;
    config.parser_version = 8;
    config.progress_every = 100;
    config.zipf_max_terms = 100000;
    config.zipf_fit_terms = 10000;
    config.top_terms = 30;
    config.lock_lease_seconds = 600;
    return config;
}

}  // namespace

Config load_config(const char *path) {
    Config config = default_config();
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) fail("не удалось открыть конфигурацию %s", path);

    char line[4096];
    std::string section;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char *content = trim(line);
        if (*content == '\0' || *content == '#') continue;
        const bool nested = line[0] == ' ' || line[0] == '\t';
        char *separator = std::strchr(content, ':');
        if (separator == nullptr) continue;
        *separator = '\0';
        char *key = trim(content);
        char *value = trim(separator + 1);
        if (!nested && *value == '\0') {
            section = key;
            continue;
        }
        if (section == "db") {
            if (std::strcmp(key, "uri") == 0) config.uri = value;
            else if (std::strcmp(key, "database") == 0) config.database = value;
            else if (std::strcmp(key, "documents_collection") == 0) config.documents = value;
            else if (std::strcmp(key, "processed_collection") == 0) config.processed = value;
            else if (std::strcmp(key, "document_terms_collection") == 0) config.document_terms = value;
            else if (std::strcmp(key, "token_frequencies_collection") == 0) config.token_frequencies = value;
            else if (std::strcmp(key, "lemma_frequencies_collection") == 0) config.lemma_frequencies = value;
            else if (std::strcmp(key, "linguistics_state_collection") == 0) config.state = value;
        } else if (section == "linguistics") {
            if (std::strcmp(key, "batch_size") == 0) config.batch_size = parse_positive(value, key);
            else if (std::strcmp(key, "minimum_words") == 0) config.minimum_words = parse_positive(value, key);
            else if (std::strcmp(key, "maximum_token_length") == 0) config.maximum_token_length = parse_positive(value, key);
            else if (std::strcmp(key, "parser_version") == 0) config.parser_version = parse_positive(value, key);
            else if (std::strcmp(key, "progress_every_documents") == 0) config.progress_every = parse_positive(value, key);
            else if (std::strcmp(key, "zipf_max_terms") == 0) config.zipf_max_terms = parse_positive(value, key);
            else if (std::strcmp(key, "zipf_fit_terms") == 0) config.zipf_fit_terms = parse_positive(value, key);
            else if (std::strcmp(key, "top_terms") == 0) config.top_terms = parse_positive(value, key);
            else if (std::strcmp(key, "lock_lease_seconds") == 0) config.lock_lease_seconds = parse_positive(value, key);
            else if (std::strcmp(key, "lemma_dictionary") == 0) config.lemma_dictionary = value;
            else if (std::strcmp(key, "lemma_vocabulary") == 0) config.lemma_vocabulary = value;
            else if (std::strcmp(key, "lemma_overrides") == 0) config.lemma_overrides = value;
            else if (std::strcmp(key, "output_directory") == 0) config.output_directory = value;
            else if (std::strcmp(key, "max_documents") == 0 && std::strcmp(value, "null") != 0) config.max_documents = parse_positive(value, key);
        }
    }
    if (std::ferror(file)) {
        std::fclose(file);
        fail("ошибка чтения конфигурации");
    }
    std::fclose(file);
    resolve_path(&config.lemma_dictionary, path);
    resolve_path(&config.lemma_vocabulary, path);
    resolve_path(&config.lemma_overrides, path);
    resolve_path(&config.output_directory, path);
    return config;
}
