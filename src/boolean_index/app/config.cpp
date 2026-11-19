// Загружает параметры построения дискового инвертированного индекса из YAML.
#include "boolean_index/app/config.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Завершает запуск индексатора с понятным сообщением о неверной конфигурации.
[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

// Удаляет пробельные символы по краям изменяемой строки конфигурации.
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

// Преобразует строку в строго положительное значение типа size_t.
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

// Относительные пути в YAML отсчитываются от каталога самого файла конфигурации.
void resolve_path(std::string *value, const std::string &config_path) {
    if (value->empty() || value->front() == '/') return;
    const size_t separator = config_path.find_last_of('/');
    if (separator == std::string::npos) return;
    *value = config_path.substr(0, separator + 1) + *value;
}

}  // namespace

// Читает настройки индекса, подставляя безопасные значения по умолчанию.
BooleanIndexConfig load_boolean_index_config(const char *path) {
    BooleanIndexConfig config{};
    config.uri = "mongodb://localhost:27017";
    config.database = "mai_ir";
    config.terms_collection = "document_terms";
    config.output_directory = "results/boolean_index";
    config.term_field = "lemmas";
    config.block_postings = 500000;
    config.progress_every_documents = 1000;
    config.minimum_skip_postings = 16;

    // Достаточный для проекта поднабор YAML разбирается без сторонней библиотеки.
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        fail("не удалось открыть файл конфигурации");
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
            } else if (std::strcmp(key, "document_terms_collection") == 0) {
                config.terms_collection = value;
            }
        } else if (section == "boolean_index") {
            if (std::strcmp(key, "output_directory") == 0) {
                config.output_directory = value;
            } else if (std::strcmp(key, "term_field") == 0) {
                config.term_field = value;
            } else if (std::strcmp(key, "block_postings") == 0) {
                valid = parse_positive_size(value, &config.block_postings);
            } else if (std::strcmp(key, "progress_every_documents") == 0) {
                valid = parse_positive_size(value,
                                            &config.progress_every_documents);
            } else if (std::strcmp(key, "minimum_skip_postings") == 0) {
                valid = parse_positive_size(value,
                                            &config.minimum_skip_postings);
            }
        }
        if (!valid) {
            std::fclose(file);
            fail("некорректное значение в файле конфигурации");
        }
    }
    std::fclose(file);
    // Индекс строится либо по исходным токенам, либо по нормализованным леммам.
    if ((config.term_field != "lemmas" && config.term_field != "tokens") ||
        config.output_directory.empty()) {
        fail("term_field должен быть lemmas или tokens");
    }
    resolve_path(&config.output_directory, path);
    return config;
}
