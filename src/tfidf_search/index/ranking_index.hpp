// Объявляет поисковый индекс с postings и нормами векторов документов.
#ifndef MAI_IR_TFIDF_SEARCH_RANKING_INDEX_HPP
#define MAI_IR_TFIDF_SEARCH_RANKING_INDEX_HPP

#include <cstddef>
#include <vector>

#include "boolean_search/index/types.hpp"

// Объединяет базовый инвертированный индекс с L2-нормой каждого документа.
struct TfidfIndex {
    BooleanIndex boolean_index;
    std::vector<double> document_norms;
};

// Открывает все файлы, необходимые для вычисления cosine score.
bool tfidf_index_open(TfidfIndex *index, const char *directory,
                      char *error, size_t error_size);

// Закрывает файлы и очищает нормы документов.
void tfidf_index_close(TfidfIndex *index);

#endif
