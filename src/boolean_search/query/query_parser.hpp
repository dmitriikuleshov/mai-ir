// Объявляет выполнение булева запроса над открытым инвертированным индексом.
#ifndef MAI_IR_BOOLEAN_SEARCH_QUERY_PARSER_HPP
#define MAI_IR_BOOLEAN_SEARCH_QUERY_PARSER_HPP

#include <cstddef>

#include "linguistics/language/pipeline.hpp"
#include "boolean_search/index/types.hpp"
#include "boolean_search/query/posting_operations.hpp"

// Нормализует и вычисляет выражение, возвращая список подходящих docID или ошибку.
BooleanResult boolean_search(BooleanIndex *index, const char *query,
                             size_t maximum_token_length,
                             const LemmaDictionary *dictionary,
                             bool use_skip_pointers,
                             IntersectionStats *stats,
                             char *error, size_t error_size);

#endif
