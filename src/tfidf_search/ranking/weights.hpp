#ifndef MAI_IR_TFIDF_SEARCH_WEIGHTS_HPP
#define MAI_IR_TFIDF_SEARCH_WEIGHTS_HPP

#include <cstdint>

double logarithmic_term_frequency(uint64_t frequency);
double inverse_document_frequency(uint32_t document_count,
                                  uint32_t document_frequency);

#endif
