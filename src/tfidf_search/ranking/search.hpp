#ifndef MAI_IR_TFIDF_SEARCH_SEARCH_HPP
#define MAI_IR_TFIDF_SEARCH_SEARCH_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "linguistics/language/pipeline.hpp"
#include "tfidf_search/index/ranking_index.hpp"

struct RankedDocument {
    uint32_t document_id;
    double score;
};

struct TfidfSearchResult {
    std::vector<RankedDocument> documents;
    size_t matched_documents;
};

TfidfSearchResult tfidf_search(TfidfIndex *index, const char *query,
                               size_t maximum_token_length,
                               size_t result_limit,
                               const LemmaDictionary *dictionary,
                               char *error, size_t error_size);

#endif
