#ifndef MAI_IR_LINGUISTICS_CORE_HPP
#define MAI_IR_LINGUISTICS_CORE_HPP

#include <cstddef>
#include <cstdint>

struct LemmaDictionary;

struct TermFrequency {
    char *term;
    uint64_t count;
};

struct FrequencyArray {
    TermFrequency *items;
    size_t size;
};

struct LinguisticResult {
    uint64_t token_count;
    uint64_t token_characters;
    uint64_t changed_token_count;
    uint64_t tokenization_nanoseconds;
    uint64_t lemmatization_nanoseconds;
    FrequencyArray tokens;
    FrequencyArray lemmas;
};

LemmaDictionary *lemma_dictionary_load(const char *path);
void lemma_dictionary_destroy(LemmaDictionary *dictionary);

LinguisticResult linguistics_process(const char *text, size_t text_size,
                                     size_t maximum_token_length,
                                     const LemmaDictionary *dictionary);
void linguistics_result_destroy(LinguisticResult *result);

#endif
