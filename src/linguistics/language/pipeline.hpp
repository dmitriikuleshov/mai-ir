#ifndef MAI_IR_LINGUISTICS_PIPELINE_HPP
#define MAI_IR_LINGUISTICS_PIPELINE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "linguistics/language/lemmatizer.hpp"

struct TermFrequency {
    std::string term;
    uint64_t count = 0;
};

using FrequencyArray = std::vector<TermFrequency>;

struct LinguisticResult {
    uint64_t token_count = 0;
    uint64_t token_characters = 0;
    uint64_t changed_token_count = 0;
    uint64_t tokenization_nanoseconds = 0;
    uint64_t lemmatization_nanoseconds = 0;
    FrequencyArray tokens;
    FrequencyArray lemmas;
};

LinguisticResult linguistics_process(const char *text, size_t text_size,
                                     size_t maximum_token_length,
                                     const LemmaDictionary *dictionary);

#endif
