#include "boolean_search/index/index_reader.hpp"

#include "boolean_index/compression/variable_byte.hpp"

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

using boolean_index_format::DocumentHeader;
using boolean_index_format::INDEX_MAGIC;
using boolean_index_format::LexiconEntry;

namespace {

FILE *as_file(void *pointer) {
    return static_cast<FILE *>(pointer);
}

void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

std::string make_path(const char *directory, const char *name) {
    return std::string(directory) + "/" + name;
}

bool read_exact(FILE *file, void *target, size_t size) {
    return std::fread(target, 1, size, file) == size;
}

bool seek_file(FILE *file, uint64_t offset) {
    return offset <= static_cast<uint64_t>(INT64_MAX) &&
           fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
}

bool compare_term(BooleanIndex *index, const LexiconEntry &entry,
                  const char *term, std::string *buffer, int *comparison) {
    buffer->resize(entry.term_length);
    FILE *terms = as_file(index->terms_file);
    if (!seek_file(terms, entry.term_offset) ||
        !read_exact(terms, buffer->data(), entry.term_length)) {
        return false;
    }
    *comparison = buffer->compare(term);
    return true;
}

}  // namespace

bool boolean_index_find_term(BooleanIndex *index, const char *term,
                             LexiconEntry *result) {
    uint64_t left = 0;
    uint64_t right = index->header.term_count;
    std::string buffer;
    FILE *lexicon = as_file(index->lexicon_file);
    while (left < right) {
        const uint64_t middle = left + (right - left) / 2;
        const uint64_t offset = sizeof(boolean_index_format::IndexHeader) +
                                middle * sizeof(LexiconEntry);
        LexiconEntry entry{};
        if (!seek_file(lexicon, offset) ||
            !read_exact(lexicon, &entry, sizeof(entry))) {
            return false;
        }
        int comparison = 0;
        if (!compare_term(index, entry, term, &buffer, &comparison)) {
            return false;
        }
        if (comparison == 0) {
            *result = entry;
            return true;
        }
        if (comparison < 0) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return false;
}

bool boolean_index_open(BooleanIndex *index, const char *directory,
                        char *error, size_t error_size) {
    *index = {};
    const char *names[] = {"lexicon.bin", "terms.bin", "postings.bin",
                           "documents.bin"};
    void **targets[] = {&index->lexicon_file, &index->terms_file,
                        &index->postings_file, &index->documents_file};
    for (size_t item = 0; item < 4; ++item) {
        const std::string path = make_path(directory, names[item]);
        FILE *file = std::fopen(path.c_str(), "rb");
        if (file == nullptr) {
            set_error(error, error_size, "не удалось открыть файлы индекса");
            boolean_index_close(index);
            return false;
        }
        *targets[item] = file;
    }
    if (!read_exact(as_file(index->lexicon_file), &index->header,
                    sizeof(index->header)) ||
        std::memcmp(index->header.magic, INDEX_MAGIC, sizeof(INDEX_MAGIC)) != 0 ||
        index->header.version != boolean_index_format::VERSION) {
        set_error(error, error_size, "неверный формат или версия индекса");
        boolean_index_close(index);
        return false;
    }
    DocumentHeader documents{};
    if (!read_exact(as_file(index->documents_file), &documents,
                    sizeof(documents)) ||
        std::memcmp(documents.magic, boolean_index_format::DOCUMENT_MAGIC,
                    sizeof(documents.magic)) != 0 ||
        documents.version != boolean_index_format::VERSION ||
        documents.document_count != index->header.document_count) {
        set_error(error, error_size,
                  "таблица документов не соответствует индексу");
        boolean_index_close(index);
        return false;
    }
    return true;
}

void boolean_index_close(BooleanIndex *index) {
    void **files[] = {&index->lexicon_file, &index->terms_file,
                      &index->postings_file, &index->documents_file};
    for (void **file : files) {
        if (*file != nullptr) {
            std::fclose(as_file(*file));
            *file = nullptr;
        }
    }
    index->header = {};
}

bool boolean_index_document_oid(BooleanIndex *index, uint32_t document_id,
                                uint8_t oid[12]) {
    if (document_id >= index->header.document_count) {
        return false;
    }
    FILE *documents = as_file(index->documents_file);
    const uint64_t offset = sizeof(DocumentHeader) +
                            static_cast<uint64_t>(document_id) * 12;
    return seek_file(documents, offset) && read_exact(documents, oid, 12);
}

WeightedPostingList load_weighted_postings(BooleanIndex *index,
                                           const char *term) {
    LexiconEntry entry{};
    if (!boolean_index_find_term(index, term, &entry) ||
        entry.postings_count == 0) {
        return {};
    }
    WeightedPostingList result;
    result.reserve(entry.postings_count);
    FILE *postings = as_file(index->postings_file);
    if (!seek_file(postings, entry.postings_offset)) {
        return {};
    }
    uint32_t document_id = 0;
    for (uint32_t position = 0; position < entry.postings_count; ++position) {
        uint32_t document_gap = 0;
        uint32_t term_frequency = 0;
        if (!variable_byte_read(postings, &document_gap) ||
            !variable_byte_read(postings, &term_frequency) ||
            term_frequency == 0 || document_gap > UINT32_MAX - document_id ||
            (position != 0 && document_gap == 0)) {
            return {};
        }
        document_id += document_gap;
        result.push_back({document_id, term_frequency});
    }
    return result;
}

BooleanResult load_postings(BooleanIndex *index, const char *term) {
    const WeightedPostingList weighted = load_weighted_postings(index, term);
    BooleanResult result;
    result.reserve(weighted.size());
    for (const WeightedPosting &posting : weighted) {
        result.push_back(posting.document_id);
    }
    return result;
}
