// Объявляет точное позиционное пересечение термов цитаты.
#ifndef MAI_IR_PHRASE_SEARCH_QUERY_SEARCH_HPP
#define MAI_IR_PHRASE_SEARCH_QUERY_SEARCH_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "linguistics/language/lemmatizer.hpp"
#include "phrase_search/index/types.hpp"

struct PhraseMatch {
    uint32_t document_id;
    std::vector<uint32_t> start_positions;
};

struct PhraseSearchResult {
    std::vector<PhraseMatch> documents;
    uint64_t occurrence_count = 0;
};

// Пересекает уже загруженные координаты; используется также изолированными тестами.
PhraseSearchResult intersect_phrase_positions(
    const std::vector<PositionPostingList> &terms);

PhraseSearchResult phrase_search(PositionalIndex *index, const char *query,
                                 size_t maximum_token_length,
                                 const LemmaDictionary *dictionary,
                                 char *error, size_t error_size);

#endif
