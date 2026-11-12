// Объявляет булевы операции над отсортированными списками docID.
#ifndef MAI_IR_BOOLEAN_SEARCH_POSTING_OPERATIONS_HPP
#define MAI_IR_BOOLEAN_SEARCH_POSTING_OPERATIONS_HPP

#include "boolean_search/index/types.hpp"

// Вычисляет пересечение posting lists, соответствующее оператору AND.
BooleanResult intersect_postings(const BooleanResult &left,
                                 const BooleanResult &right);

// Вычисляет объединение posting lists, соответствующее оператору OR.
BooleanResult unite_postings(const BooleanResult &left,
                             const BooleanResult &right);

// Строит дополнение списка относительно всех документов индекса для NOT.
BooleanResult complement_postings(const BooleanResult &input,
                                  uint32_t document_count);

#endif
