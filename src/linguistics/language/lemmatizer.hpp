#ifndef MAI_IR_LINGUISTICS_LEMMATIZER_HPP
#define MAI_IR_LINGUISTICS_LEMMATIZER_HPP

#include "linguistics/language/tokenizer.hpp"

struct LemmaDictionary;

struct LemmatizationResult {
    TokenList lemmas;
    uint64_t changed_token_count = 0;
    uint64_t elapsed_nanoseconds = 0;
};

LemmaDictionary *lemma_dictionary_load(
    const char *exceptions_path, const char *lemmas_path = nullptr,
    const char *overrides_path = nullptr);
void lemma_dictionary_destroy(LemmaDictionary *dictionary);

LemmatizationResult lemmatize_tokens(const TokenList *tokens,
                                     const LemmaDictionary *dictionary);

#endif
