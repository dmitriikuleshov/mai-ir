#ifndef MAI_IR_TFIDF_SEARCH_RANKING_INDEX_HPP
#define MAI_IR_TFIDF_SEARCH_RANKING_INDEX_HPP

#include <cstddef>
#include <vector>

#include "boolean_search/index/types.hpp"

struct TfidfIndex {
    BooleanIndex boolean_index;
    std::vector<double> document_norms;
};

bool tfidf_index_open(TfidfIndex *index, const char *directory,
                      char *error, size_t error_size);
void tfidf_index_close(TfidfIndex *index);

#endif
