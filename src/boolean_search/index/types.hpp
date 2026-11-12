#ifndef MAI_IR_BOOLEAN_SEARCH_TYPES_HPP
#define MAI_IR_BOOLEAN_SEARCH_TYPES_HPP

#include <cstdint>
#include <vector>

#include "boolean_index/storage/format.hpp"

using BooleanResult = std::vector<uint32_t>;

struct WeightedPosting {
    uint32_t document_id;
    uint32_t term_frequency;
};

using WeightedPostingList = std::vector<WeightedPosting>;

struct BooleanIndex {
    void *lexicon_file;
    void *terms_file;
    void *postings_file;
    void *documents_file;
    boolean_index_format::IndexHeader header;
};

#endif
