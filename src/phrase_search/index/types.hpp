// Описывает открытый координатный индекс и позиции термов в документах.
#ifndef MAI_IR_PHRASE_SEARCH_TYPES_HPP
#define MAI_IR_PHRASE_SEARCH_TYPES_HPP

#include <cstdint>
#include <vector>

#include "positional_index/storage/format.hpp"

struct PositionPosting {
    uint32_t document_id;
    std::vector<uint32_t> positions;
};

using PositionPostingList = std::vector<PositionPosting>;

struct PositionalIndex {
    void *lexicon_file = nullptr;
    void *terms_file = nullptr;
    void *coordinates_file = nullptr;
    void *documents_file = nullptr;
    positional_index_format::Header header{};
};

#endif
