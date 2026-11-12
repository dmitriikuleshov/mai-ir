#ifndef MAI_IR_BOOLEAN_SEARCH_POSTING_OPERATIONS_HPP
#define MAI_IR_BOOLEAN_SEARCH_POSTING_OPERATIONS_HPP

#include "boolean_search/index/types.hpp"

BooleanResult intersect_postings(const BooleanResult &left,
                                 const BooleanResult &right);
BooleanResult unite_postings(const BooleanResult &left,
                             const BooleanResult &right);
BooleanResult complement_postings(const BooleanResult &input,
                                  uint32_t document_count);

#endif
