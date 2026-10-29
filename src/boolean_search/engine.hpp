#ifndef MAI_IR_BOOLEAN_SEARCH_ENGINE_HPP
#define MAI_IR_BOOLEAN_SEARCH_ENGINE_HPP

#include <cstddef>
#include <cstdint>

#include "../boolean_index/format.hpp"
#include "../linguistics/core.hpp"

struct BooleanResult {
    uint32_t *document_ids;
    size_t size;
};

struct BooleanIndex {
    void *lexicon_file;
    void *terms_file;
    void *postings_file;
    void *documents_file;
    boolean_index_format::IndexHeader header;
};

bool boolean_index_open(BooleanIndex *index, const char *directory,
                        char *error, size_t error_size);
void boolean_index_close(BooleanIndex *index);

bool boolean_index_document_oid(BooleanIndex *index, uint32_t document_id,
                                uint8_t oid[12]);

BooleanResult boolean_search(BooleanIndex *index, const char *query,
                             size_t maximum_token_length,
                             const LemmaDictionary *dictionary,
                             char *error, size_t error_size);
void boolean_result_destroy(BooleanResult *result);

#endif
