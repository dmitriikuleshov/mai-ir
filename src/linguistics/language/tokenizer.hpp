#ifndef MAI_IR_LINGUISTICS_TOKENIZER_HPP
#define MAI_IR_LINGUISTICS_TOKENIZER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using TokenList = std::vector<std::string>;

struct TokenizationResult {
    TokenList tokens;
    uint64_t token_characters = 0;
    uint64_t elapsed_nanoseconds = 0;
};

TokenizationResult tokenize_text(const char *text, size_t text_size,
                                 size_t maximum_token_length);

#endif
