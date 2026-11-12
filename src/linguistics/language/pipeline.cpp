#include "linguistics/language/pipeline.hpp"

#include <algorithm>
#include <utility>

namespace {

FrequencyArray make_frequencies(TokenList tokens) {
    std::sort(tokens.begin(), tokens.end());

    FrequencyArray frequencies;
    frequencies.reserve(tokens.size());
    size_t position = 0;
    while (position < tokens.size()) {
        size_t next = position + 1;
        while (next < tokens.size() && tokens[next] == tokens[position]) {
            ++next;
        }
        frequencies.push_back(
            {std::move(tokens[position]), static_cast<uint64_t>(next - position)});
        position = next;
    }
    return frequencies;
}

}  // namespace

LinguisticResult linguistics_process(const char *text, size_t text_size,
                                     size_t maximum_token_length,
                                     const LemmaDictionary *dictionary) {
    TokenizationResult tokenization =
        tokenize_text(text, text_size, maximum_token_length);
    LemmatizationResult lemmatization =
        lemmatize_tokens(&tokenization.tokens, dictionary);

    LinguisticResult result;
    result.token_count = tokenization.tokens.size();
    result.token_characters = tokenization.token_characters;
    result.changed_token_count = lemmatization.changed_token_count;
    result.tokenization_nanoseconds = tokenization.elapsed_nanoseconds;
    result.lemmatization_nanoseconds = lemmatization.elapsed_nanoseconds;
    result.tokens = make_frequencies(std::move(tokenization.tokens));
    result.lemmas = make_frequencies(std::move(lemmatization.lemmas));
    return result;
}

