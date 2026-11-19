// Проверяет бинарный формат и декодирует docID-gap и position-gap.
#include "phrase_search/index/index_reader.hpp"

#include "boolean_index/compression/variable_byte.hpp"
#include "boolean_index/storage/format.hpp"

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace {

FILE *as_file(void *pointer) { return static_cast<FILE *>(pointer); }

void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

std::string path_join(const char *directory, const char *name) {
    return std::string(directory) + "/" + name;
}

bool seek_file(FILE *file, uint64_t offset) {
    return offset <= static_cast<uint64_t>(INT64_MAX) &&
           fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
}

bool read_exact(FILE *file, void *target, size_t size) {
    return std::fread(target, 1, size, file) == size;
}

bool compare_term(PositionalIndex *index,
                  const positional_index_format::LexiconEntry &entry,
                  const char *term, std::string *buffer, int *comparison) {
    buffer->resize(entry.term_length);
    if (!seek_file(as_file(index->terms_file), entry.term_offset) ||
        !read_exact(as_file(index->terms_file), buffer->data(),
                    entry.term_length)) {
        return false;
    }
    *comparison = buffer->compare(term);
    return true;
}

bool find_term(PositionalIndex *index, const char *term,
               positional_index_format::LexiconEntry *result,
               bool *found) {
    *found = false;
    uint64_t left = 0;
    uint64_t right = index->header.term_count;
    std::string buffer;
    while (left < right) {
        const uint64_t middle = left + (right - left) / 2;
        const uint64_t offset = sizeof(positional_index_format::Header) +
                                middle *
                                    sizeof(positional_index_format::LexiconEntry);
        positional_index_format::LexiconEntry entry{};
        if (!seek_file(as_file(index->lexicon_file), offset) ||
            !read_exact(as_file(index->lexicon_file), &entry, sizeof(entry))) {
            return false;
        }
        int comparison = 0;
        if (!compare_term(index, entry, term, &buffer, &comparison)) return false;
        if (comparison == 0) {
            *result = entry;
            *found = true;
            return true;
        }
        if (comparison < 0) left = middle + 1;
        else right = middle;
    }
    return true;
}

}  // namespace

bool positional_index_open(PositionalIndex *index, const char *directory,
                           const char *boolean_index_directory,
                           char *error, size_t error_size) {
    *index = {};
    if (error != nullptr && error_size != 0) error[0] = '\0';
    const std::string lexicon = path_join(directory, "coordinate_lexicon.bin");
    const std::string terms = path_join(directory, "coordinate_terms.bin");
    const std::string coordinates = path_join(directory, "coordinates.bin");
    const std::string documents =
        path_join(boolean_index_directory, "documents.bin");
    index->lexicon_file = std::fopen(lexicon.c_str(), "rb");
    index->terms_file = std::fopen(terms.c_str(), "rb");
    index->coordinates_file = std::fopen(coordinates.c_str(), "rb");
    index->documents_file = std::fopen(documents.c_str(), "rb");
    if (index->lexicon_file == nullptr || index->terms_file == nullptr ||
        index->coordinates_file == nullptr || index->documents_file == nullptr) {
        set_error(error, error_size,
                  "не удалось открыть файлы координатного индекса");
        positional_index_close(index);
        return false;
    }
    if (!read_exact(as_file(index->lexicon_file), &index->header,
                    sizeof(index->header)) ||
        std::memcmp(index->header.magic, positional_index_format::MAGIC,
                    sizeof(index->header.magic)) != 0 ||
        index->header.version != positional_index_format::VERSION) {
        set_error(error, error_size,
                  "неверный формат координатного индекса");
        positional_index_close(index);
        return false;
    }
    boolean_index_format::DocumentHeader document_header{};
    if (!read_exact(as_file(index->documents_file), &document_header,
                    sizeof(document_header)) ||
        std::memcmp(document_header.magic,
                    boolean_index_format::DOCUMENT_MAGIC,
                    sizeof(document_header.magic)) != 0 ||
        document_header.version != boolean_index_format::VERSION ||
        document_header.document_count != index->header.document_count) {
        set_error(error, error_size,
                  "documents.bin не соответствует координатному индексу");
        positional_index_close(index);
        return false;
    }
    return true;
}

void positional_index_close(PositionalIndex *index) {
    void **files[] = {&index->lexicon_file, &index->terms_file,
                      &index->coordinates_file, &index->documents_file};
    for (void **value : files) {
        if (*value != nullptr) {
            std::fclose(as_file(*value));
            *value = nullptr;
        }
    }
    index->header = {};
}

bool load_position_postings(PositionalIndex *index, const char *term,
                            PositionPostingList *result, bool *found) {
    result->clear();
    positional_index_format::LexiconEntry entry{};
    if (!find_term(index, term, &entry, found)) return false;
    if (!*found) return true;
    if (entry.postings_count == 0 ||
        !seek_file(as_file(index->coordinates_file), entry.postings_offset)) {
        return false;
    }
    result->reserve(entry.postings_count);
    uint32_t document_id = 0;
    uint64_t positions_read = 0;
    for (uint32_t posting_index = 0; posting_index < entry.postings_count;
         ++posting_index) {
        uint32_t document_gap = 0;
        uint32_t position_count = 0;
        if (!variable_byte_read(as_file(index->coordinates_file), &document_gap) ||
            !variable_byte_read(as_file(index->coordinates_file),
                                &position_count) ||
            (posting_index != 0 && document_gap == 0) || position_count == 0 ||
            document_gap > UINT32_MAX - document_id) {
            result->clear();
            return false;
        }
        document_id += document_gap;
        if (document_id >= index->header.document_count ||
            positions_read > UINT64_MAX - position_count) {
            result->clear();
            return false;
        }
        PositionPosting posting{document_id, {}};
        posting.positions.reserve(position_count);
        uint32_t position = 0;
        for (uint32_t item = 0; item < position_count; ++item) {
            uint32_t gap = 0;
            if (!variable_byte_read(as_file(index->coordinates_file), &gap) ||
                (item != 0 && gap == 0) || gap > UINT32_MAX - position) {
                result->clear();
                return false;
            }
            position += gap;
            posting.positions.push_back(position);
        }
        positions_read += position_count;
        result->push_back(std::move(posting));
    }
    if (positions_read != entry.position_count) {
        result->clear();
        return false;
    }
    return true;
}

bool positional_index_document_oid(PositionalIndex *index,
                                   uint32_t document_id, uint8_t oid[12]) {
    if (document_id >= index->header.document_count) return false;
    const uint64_t offset =
        sizeof(boolean_index_format::DocumentHeader) +
        static_cast<uint64_t>(document_id) * 12;
    return seek_file(as_file(index->documents_file), offset) &&
           read_exact(as_file(index->documents_file), oid, 12);
}
