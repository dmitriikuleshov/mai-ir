#ifndef MAI_IR_BOOLEAN_SEARCH_INDEX_READER_HPP
#define MAI_IR_BOOLEAN_SEARCH_INDEX_READER_HPP

#include <cstddef>
#include <cstdint>

#include "boolean_search/index/types.hpp"

bool boolean_index_open(BooleanIndex *index, const char *directory,
                        char *error, size_t error_size);
void boolean_index_close(BooleanIndex *index);

bool boolean_index_document_oid(BooleanIndex *index, uint32_t document_id,
                                uint8_t oid[12]);
bool boolean_index_find_term(
    BooleanIndex *index, const char *term,
    boolean_index_format::LexiconEntry *result);

BooleanResult load_postings(BooleanIndex *index, const char *term);
WeightedPostingList load_weighted_postings(BooleanIndex *index,
                                           const char *term);

#endif
