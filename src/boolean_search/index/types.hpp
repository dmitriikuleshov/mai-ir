// Описывает представление открытого индекса и списков найденных документов.
#ifndef MAI_IR_BOOLEAN_SEARCH_TYPES_HPP
#define MAI_IR_BOOLEAN_SEARCH_TYPES_HPP

#include <cstdint>
#include <vector>

#include "boolean_index/storage/format.hpp"

// Булев результат всегда остаётся отсортированным списком уникальных docID.
using BooleanResult = std::vector<uint32_t>;

// Posting для ранжирования хранит docID и число вхождений терма в документ.
struct WeightedPosting {
    uint32_t document_id;
    uint32_t term_frequency;
};

using WeightedPostingList = std::vector<WeightedPosting>;

// Содержит открытые бинарные файлы и проверенный общий заголовок индекса.
struct BooleanIndex {
    void *lexicon_file;
    void *terms_file;
    void *postings_file;
    void *documents_file;
    boolean_index_format::IndexHeader header;
};

#endif
