// Объявляет структуры результата и функцию ранжированного поиска TF-IDF.
#ifndef MAI_IR_TFIDF_SEARCH_SEARCH_HPP
#define MAI_IR_TFIDF_SEARCH_SEARCH_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "linguistics/language/pipeline.hpp"
#include "tfidf_search/index/ranking_index.hpp"

// Связывает внутренний docID с рассчитанной косинусной близостью.
struct RankedDocument {
    uint32_t document_id;
    double score;
};

// Содержит ограниченную выдачу и полное число документов с ненулевым score.
struct TfidfSearchResult {
    std::vector<RankedDocument> documents;
    size_t matched_documents;
};

// Выполняет поиск по естественному запросу и возвращает документы по убыванию score.
TfidfSearchResult tfidf_search(TfidfIndex *index, const char *query,
                               size_t maximum_token_length,
                               size_t result_limit,
                               const LemmaDictionary *dictionary,
                               char *error, size_t error_size);

#endif
