// Объявляет функции весов, общие для построения индекса и обработки запроса.
#ifndef MAI_IR_TFIDF_SEARCH_WEIGHTS_HPP
#define MAI_IR_TFIDF_SEARCH_WEIGHTS_HPP

#include <cstdint>

// Возвращает логарифмически сглаженную частоту терма.
double logarithmic_term_frequency(uint64_t frequency);

// Возвращает обратную документную частоту по размерам корпуса и posting list.
double inverse_document_frequency(uint32_t document_count,
                                  uint32_t document_frequency);

#endif
