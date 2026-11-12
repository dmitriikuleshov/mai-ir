// Объявляет публичные структуры и функцию токенизации технического текста.
#ifndef MAI_IR_LINGUISTICS_TOKENIZER_HPP
#define MAI_IR_LINGUISTICS_TOKENIZER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using TokenList = std::vector<std::string>;

// Содержит токены, их суммарную длину и время выполнения токенизации.
struct TokenizationResult {
    TokenList tokens;
    uint64_t token_characters = 0;
    uint64_t elapsed_nanoseconds = 0;
};

// Преобразует заданный фрагмент текста в нормализованный список токенов.
TokenizationResult tokenize_text(const char *text, size_t text_size,
                                 size_t maximum_token_length);

#endif
