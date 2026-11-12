// Объявляет словарь, результат и публичные функции лемматизации токенов.
#ifndef MAI_IR_LINGUISTICS_LEMMATIZER_HPP
#define MAI_IR_LINGUISTICS_LEMMATIZER_HPP

#include "linguistics/language/tokenizer.hpp"

struct LemmaDictionary;

// Содержит леммы, число изменённых токенов и время выполнения обработки.
struct LemmatizationResult {
    TokenList lemmas;
    uint64_t changed_token_count = 0;
    uint64_t elapsed_nanoseconds = 0;
};

// Загружает словарные исключения, допустимые леммы и настройки технического корпуса.
LemmaDictionary *lemma_dictionary_load(
    const char *exceptions_path, const char *lemmas_path = nullptr,
    const char *overrides_path = nullptr);

// Освобождает ранее загруженный словарь лемматизации.
void lemma_dictionary_destroy(LemmaDictionary *dictionary);

// Преобразует токены в леммы и возвращает статистику обработки.
LemmatizationResult lemmatize_tokens(const TokenList *tokens,
                                     const LemmaDictionary *dictionary);

#endif
