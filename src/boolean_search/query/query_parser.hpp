#ifndef MAI_IR_BOOLEAN_SEARCH_QUERY_PARSER_HPP
#define MAI_IR_BOOLEAN_SEARCH_QUERY_PARSER_HPP

#include <cstddef>

#include "linguistics/language/pipeline.hpp"
#include "boolean_search/index/types.hpp"

BooleanResult boolean_search(BooleanIndex *index, const char *query,
                             size_t maximum_token_length,
                             const LemmaDictionary *dictionary,
                             char *error, size_t error_size);

#endif
