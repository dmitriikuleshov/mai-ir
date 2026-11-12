// Реализует общие формулы логарифмического TF и обратной документной частоты.
#include "tfidf_search/ranking/weights.hpp"

#include <cmath>

// Вычисляет l-вес терма: 0 для отсутствующего терма, иначе 1 + log10(tf).
double logarithmic_term_frequency(uint64_t frequency) {
    return frequency == 0
               ? 0.0
               : 1.0 + std::log10(static_cast<double>(frequency));
}

// Вычисляет t-вес терма: IDF = log10(N / df).
double inverse_document_frequency(uint32_t document_count,
                                  uint32_t document_frequency) {
    // Невозможные частоты дают нулевой вес вместо деления на ноль.
    if (document_count == 0 || document_frequency == 0 ||
        document_frequency > document_count) {
        return 0.0;
    }
    return std::log10(static_cast<double>(document_count) /
                      static_cast<double>(document_frequency));
}
