#include "tfidf_search/index/ranking_index.hpp"

#include "boolean_search/index/index_reader.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

}  // namespace

bool tfidf_index_open(TfidfIndex *index, const char *directory,
                      char *error, size_t error_size) {
    *index = {};
    if (!boolean_index_open(&index->boolean_index, directory, error,
                            error_size)) {
        return false;
    }

    const std::string path = std::string(directory) + "/norms.bin";
    FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        set_error(error, error_size,
                  "не удалось открыть таблицу норм документов");
        tfidf_index_close(index);
        return false;
    }

    boolean_index_format::DocumentNormHeader header{};
    if (std::fread(&header, sizeof(header), 1, file) != 1 ||
        std::memcmp(header.magic, boolean_index_format::DOCUMENT_NORM_MAGIC,
                    sizeof(header.magic)) != 0 ||
        header.version != boolean_index_format::VERSION ||
        header.document_count != index->boolean_index.header.document_count) {
        std::fclose(file);
        set_error(error, error_size,
                  "таблица норм документов не соответствует индексу");
        tfidf_index_close(index);
        return false;
    }

    index->document_norms.resize(header.document_count);
    const size_t bytes = index->document_norms.size() * sizeof(double);
    if (bytes != 0 &&
        std::fread(index->document_norms.data(), 1, bytes, file) != bytes) {
        std::fclose(file);
        set_error(error, error_size, "таблица норм документов повреждена");
        tfidf_index_close(index);
        return false;
    }
    if (std::fclose(file) != 0) {
        set_error(error, error_size,
                  "не удалось закрыть таблицу норм документов");
        tfidf_index_close(index);
        return false;
    }
    for (double norm : index->document_norms) {
        if (!std::isfinite(norm) || norm < 0.0) {
            set_error(error, error_size,
                      "таблица норм документов содержит неверное значение");
            tfidf_index_close(index);
            return false;
        }
    }
    return true;
}

void tfidf_index_close(TfidfIndex *index) {
    boolean_index_close(&index->boolean_index);
    index->document_norms.clear();
}
