#include "tfidf_search/ranking/weights.hpp"

#include <cmath>

double logarithmic_term_frequency(uint64_t frequency) {
    return frequency == 0
               ? 0.0
               : 1.0 + std::log10(static_cast<double>(frequency));
}

double inverse_document_frequency(uint32_t document_count,
                                  uint32_t document_frequency) {
    if (document_count == 0 || document_frequency == 0 ||
        document_frequency > document_count) {
        return 0.0;
    }
    return std::log10(static_cast<double>(document_count) /
                      static_cast<double>(document_frequency));
}
