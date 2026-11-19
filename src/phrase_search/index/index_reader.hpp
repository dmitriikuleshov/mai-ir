// Объявляет открытие координатного индекса и загрузку списков позиций.
#ifndef MAI_IR_PHRASE_SEARCH_INDEX_READER_HPP
#define MAI_IR_PHRASE_SEARCH_INDEX_READER_HPP

#include <cstddef>
#include <cstdint>

#include "phrase_search/index/types.hpp"

bool positional_index_open(PositionalIndex *index, const char *directory,
                           const char *boolean_index_directory,
                           char *error, size_t error_size);
void positional_index_close(PositionalIndex *index);

// found=false означает отсутствие терма; false как результат означает ошибку файла.
bool load_position_postings(PositionalIndex *index, const char *term,
                            PositionPostingList *result, bool *found);

bool positional_index_document_oid(PositionalIndex *index,
                                   uint32_t document_id, uint8_t oid[12]);

#endif
