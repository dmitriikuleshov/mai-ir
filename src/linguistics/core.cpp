#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "core.hpp"

// The course rules prohibit STL in the main search components. This file uses
// plain dynamically allocated arrays and qsort from the C standard library.

struct WordArray {
    char **items;
    size_t size;
    size_t capacity;
};

struct Buffer {
    char *data;
    size_t size;
    size_t capacity;
};

struct LemmaPair {
    const char *form;
    const char *lemma;
};

struct LemmaEntry {
    char *form;
    char *lemma;
};

struct LemmaDictionary {
    LemmaEntry *entries;
    size_t size;
    size_t capacity;
};

static const LemmaPair IRREGULAR_LEMMAS[] = {
    {"analyses", "analysis"}, {"are", "be"},         {"as", "as"},
    {"axes", "axe"},         {"been", "be"},        {"best", "good"},
    {"better", "good"},      {"children", "child"}, {"does", "do"},
    {"feet", "foot"},        {"gone", "go"},        {"had", "have"},
    {"has", "have"},         {"his", "his"},        {"indices", "index"},
    {"is", "be"},            {"men", "man"},        {"mice", "mouse"},
    {"news", "news"},        {"ran", "run"},        {"saw", "see"},
    {"series", "series"},    {"species", "species"},
    {"teeth", "tooth"},      {"this", "this"},      {"thus", "thus"},
    {"used", "use"},         {"using", "use"},      {"was", "be"},
    {"went", "go"},          {"were", "be"},        {"women", "woman"},
    {"worse", "bad"},        {"worst", "bad"},      {"yes", "yes"},
};

static void fail(const char *message) {
    std::fprintf(stderr, "linguistics_core: %s\n", message);
    std::exit(1);
}

static void *checked_realloc(void *pointer, size_t size) {
    void *result = std::realloc(pointer, size);
    if (result == nullptr) {
        fail("out of memory");
    }
    return result;
}

static char *duplicate_range(const char *start, size_t length) {
    char *result = static_cast<char *>(std::malloc(length + 1));
    if (result == nullptr) {
        fail("out of memory");
    }
    std::memcpy(result, start, length);
    result[length] = '\0';
    return result;
}

static void dictionary_push(LemmaDictionary *dictionary, char *form, char *lemma) {
    if (dictionary->size == dictionary->capacity) {
        const size_t next_capacity =
            dictionary->capacity == 0 ? 1024 : dictionary->capacity * 2;
        dictionary->entries = static_cast<LemmaEntry *>(checked_realloc(
            dictionary->entries, next_capacity * sizeof(LemmaEntry)));
        dictionary->capacity = next_capacity;
    }
    dictionary->entries[dictionary->size++] = {form, lemma};
}

static int compare_lemma_entries(const void *left, const void *right) {
    const LemmaEntry *left_entry = static_cast<const LemmaEntry *>(left);
    const LemmaEntry *right_entry = static_cast<const LemmaEntry *>(right);
    return std::strcmp(left_entry->form, right_entry->form);
}

static LemmaDictionary dictionary_load(const char *path) {
    LemmaDictionary dictionary = {nullptr, 0, 0};
    if (path == nullptr || *path == '\0') {
        return dictionary;
    }
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        fail("cannot open lemma dictionary");
    }
    char line[4096];
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        char *separator = std::strchr(line, '\t');
        if (separator == nullptr) {
            continue;
        }
        *separator = '\0';
        char *lemma = separator + 1;
        lemma[std::strcspn(lemma, "\r\n")] = '\0';
        if (*line != '\0' && *lemma != '\0') {
            dictionary_push(&dictionary, duplicate_range(line, std::strlen(line)),
                            duplicate_range(lemma, std::strlen(lemma)));
        }
    }
    if (std::ferror(file)) {
        std::fclose(file);
        fail("cannot read lemma dictionary");
    }
    std::fclose(file);
    std::qsort(dictionary.entries, dictionary.size, sizeof(LemmaEntry),
               compare_lemma_entries);
    return dictionary;
}

static const char *dictionary_find(const LemmaDictionary *dictionary,
                                   const char *form) {
    size_t left = 0;
    size_t right = dictionary->size;
    while (left < right) {
        const size_t middle = left + (right - left) / 2;
        const int comparison = std::strcmp(form, dictionary->entries[middle].form);
        if (comparison == 0) {
            return dictionary->entries[middle].lemma;
        }
        if (comparison < 0) {
            right = middle;
        } else {
            left = middle + 1;
        }
    }
    return nullptr;
}

static void dictionary_free(LemmaDictionary *dictionary) {
    for (size_t index = 0; index < dictionary->size; ++index) {
        std::free(dictionary->entries[index].form);
        std::free(dictionary->entries[index].lemma);
    }
    std::free(dictionary->entries);
    dictionary->entries = nullptr;
    dictionary->size = 0;
    dictionary->capacity = 0;
}

static void words_push(WordArray *words, char *word) {
    if (words->size == words->capacity) {
        const size_t next_capacity = words->capacity == 0 ? 256 : words->capacity * 2;
        words->items = static_cast<char **>(checked_realloc(
            words->items, next_capacity * sizeof(char *)));
        words->capacity = next_capacity;
    }
    words->items[words->size++] = word;
}

static void words_free(WordArray *words) {
    for (size_t index = 0; index < words->size; ++index) {
        std::free(words->items[index]);
    }
    std::free(words->items);
    words->items = nullptr;
    words->size = 0;
    words->capacity = 0;
}

static void buffer_reserve(Buffer *buffer, size_t extra) {
    if (buffer->size + extra <= buffer->capacity) {
        return;
    }
    size_t next_capacity = buffer->capacity == 0 ? 4096 : buffer->capacity;
    while (next_capacity < buffer->size + extra) {
        next_capacity *= 2;
    }
    buffer->data = static_cast<char *>(
        checked_realloc(buffer->data, next_capacity));
    buffer->capacity = next_capacity;
}

static void buffer_append(Buffer *buffer, const char *value, size_t length) {
    buffer_reserve(buffer, length);
    std::memcpy(buffer->data + buffer->size, value, length);
    buffer->size += length;
}

static void buffer_append_text(Buffer *buffer, const char *value) {
    buffer_append(buffer, value, std::strlen(value));
}

static uint32_t decode_utf8(const unsigned char *text, size_t remaining,
                            size_t *width) {
    const unsigned char first = text[0];
    if (first < 0x80) {
        *width = 1;
        return first;
    }
    if ((first & 0xE0) == 0xC0 && remaining >= 2) {
        *width = 2;
        return ((first & 0x1F) << 6) | (text[1] & 0x3F);
    }
    if ((first & 0xF0) == 0xE0 && remaining >= 3) {
        *width = 3;
        return ((first & 0x0F) << 12) | ((text[1] & 0x3F) << 6) |
               (text[2] & 0x3F);
    }
    if ((first & 0xF8) == 0xF0 && remaining >= 4) {
        *width = 4;
        return ((first & 0x07) << 18) | ((text[1] & 0x3F) << 12) |
               ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
    }
    *width = 1;
    return first;
}

static bool unicode_separator(uint32_t codepoint) {
    if (codepoint <= 0x20 || codepoint == 0x7F || codepoint == 0xA0) {
        return true;
    }
    return (codepoint >= 0x2000 && codepoint <= 0x206F) ||
           (codepoint >= 0x2190 && codepoint <= 0x2BFF) ||
           (codepoint >= 0x3000 && codepoint <= 0x303F);
}

static bool unicode_space(uint32_t codepoint) {
    return codepoint <= 0x20 || codepoint == 0x7F || codepoint == 0xA0 ||
           (codepoint >= 0x2000 && codepoint <= 0x200A) ||
           codepoint == 0x2028 || codepoint == 0x2029 || codepoint == 0x202F ||
           codepoint == 0x205F || codepoint == 0x3000;
}

static Buffer normalize_input(const char *text, size_t text_size) {
    Buffer normalized = {nullptr, 0, 0};
    size_t position = 0;
    while (position < text_size) {
        size_t width = 1;
        uint32_t codepoint = decode_utf8(
            reinterpret_cast<const unsigned char *>(text + position),
            text_size - position, &width);
        if (unicode_space(codepoint)) {
            buffer_append_text(&normalized, " ");
        } else if (codepoint == 0x2018 || codepoint == 0x2019) {
            buffer_append_text(&normalized, "'");
        } else if (codepoint == 0x2010 || codepoint == 0x2011 ||
                   codepoint == 0x2012 || codepoint == 0x2013 ||
                   codepoint == 0x2014) {
            buffer_append_text(&normalized, "-");
        } else if (codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
            const char ascii = static_cast<char>(codepoint - 0xFEE0);
            buffer_append(&normalized, &ascii, 1);
        } else {
            buffer_append(&normalized, text + position, width);
        }
        position += width;
    }
    return normalized;
}

static bool word_character(const unsigned char *text, size_t remaining,
                           size_t *width) {
    uint32_t codepoint = decode_utf8(text, remaining, width);
    if (codepoint < 0x80) {
        return std::isalnum(static_cast<unsigned char>(codepoint)) != 0;
    }
    return !unicode_separator(codepoint);
}

static bool inner_separator(unsigned char value) {
    return value == '.' || value == '_' || value == '\'' || value == '-';
}

static WordArray tokenize(const char *text, size_t text_size, size_t maximum_length,
                          uint64_t *token_characters) {
    WordArray result = {nullptr, 0, 0};
    char *token = static_cast<char *>(std::malloc(text_size + 4));
    if (token == nullptr) {
        fail("out of memory");
    }

    size_t position = 0;
    while (position < text_size) {
        size_t width = 1;
        bool starts_with_dot = text[position] == '.' && position + 1 < text_size;
        size_t next_width = 1;
        if (starts_with_dot) {
            starts_with_dot = word_character(
                reinterpret_cast<const unsigned char *>(text + position + 1),
                text_size - position - 1, &next_width);
        }
        if (!starts_with_dot &&
            !word_character(reinterpret_cast<const unsigned char *>(text + position),
                            text_size - position, &width)) {
            position += width;
            continue;
        }

        size_t output_size = 0;
        size_t characters = 0;
        if (starts_with_dot) {
            token[output_size++] = '.';
            ++position;
            ++characters;
        }

        while (position < text_size) {
            size_t character_width = 1;
            const bool is_word = word_character(
                reinterpret_cast<const unsigned char *>(text + position),
                text_size - position, &character_width);
            if (is_word) {
                if (character_width == 1) {
                    token[output_size++] = static_cast<char>(std::tolower(
                        static_cast<unsigned char>(text[position])));
                } else {
                    std::memcpy(token + output_size, text + position, character_width);
                    output_size += character_width;
                }
                position += character_width;
                ++characters;
                continue;
            }

            if (character_width == 1 && inner_separator(text[position]) &&
                position + 1 < text_size) {
                size_t following_width = 1;
                if (word_character(
                        reinterpret_cast<const unsigned char *>(text + position + 1),
                        text_size - position - 1, &following_width)) {
                    token[output_size++] = text[position++];
                    ++characters;
                    continue;
                }
            }
            break;
        }

        if (position + 1 < text_size && text[position] == '+' &&
            text[position + 1] == '+') {
            token[output_size++] = '+';
            token[output_size++] = '+';
            position += 2;
            characters += 2;
        } else if (position < text_size && text[position] == '#') {
            token[output_size++] = '#';
            ++position;
            ++characters;
        }

        if (output_size >= 2 && token[output_size - 2] == '\'' &&
            token[output_size - 1] == 's') {
            output_size -= 2;
            characters -= 2;
        }
        token[output_size] = '\0';
        if (output_size > 0 && characters <= maximum_length) {
            words_push(&result, duplicate_range(token, output_size));
            *token_characters += characters;
        }
    }

    std::free(token);
    return result;
}

static bool ends_with(const char *word, size_t length, const char *suffix) {
    const size_t suffix_length = std::strlen(suffix);
    return length >= suffix_length &&
           std::memcmp(word + length - suffix_length, suffix, suffix_length) == 0;
}

static bool ascii_word(const char *word) {
    for (const unsigned char *cursor = reinterpret_cast<const unsigned char *>(word);
         *cursor != '\0'; ++cursor) {
        if (!std::isalpha(*cursor)) {
            return false;
        }
    }
    return true;
}

static bool consonant(char value) {
    if (value < 'a' || value > 'z') {
        return false;
    }
    return value != 'a' && value != 'e' && value != 'i' && value != 'o' &&
           value != 'u';
}

static char *lemmatize(const char *word, const LemmaDictionary *dictionary) {
    if (!ascii_word(word)) {
        return duplicate_range(word, std::strlen(word));
    }
    for (const LemmaPair &pair : IRREGULAR_LEMMAS) {
        if (std::strcmp(word, pair.form) == 0) {
            return duplicate_range(pair.lemma, std::strlen(pair.lemma));
        }
    }
    const char *dictionary_lemma = dictionary_find(dictionary, word);
    if (dictionary_lemma != nullptr) {
        return duplicate_range(dictionary_lemma, std::strlen(dictionary_lemma));
    }

    const size_t length = std::strlen(word);
    char *lemma = duplicate_range(word, length);
    size_t lemma_length = length;
    if (length > 4 && ends_with(word, length, "ies")) {
        lemma[length - 3] = 'y';
        lemma[length - 2] = '\0';
        return lemma;
    }
    if (length > 5 && ends_with(word, length, "ing")) {
        lemma_length -= 3;
        lemma[lemma_length] = '\0';
        if (lemma_length >= 2 && lemma[lemma_length - 1] == lemma[lemma_length - 2] &&
            consonant(lemma[lemma_length - 1])) {
            lemma[--lemma_length] = '\0';
        } else if (lemma[lemma_length - 1] == 'v' ||
                   ends_with(lemma, lemma_length, "mak") ||
                   ends_with(lemma, lemma_length, "creat")) {
            lemma[lemma_length++] = 'e';
            lemma[lemma_length] = '\0';
        }
        return lemma;
    }
    if (length > 4 && ends_with(word, length, "ed")) {
        lemma_length -= 2;
        lemma[lemma_length] = '\0';
        if (lemma_length >= 2 && lemma[lemma_length - 1] == lemma[lemma_length - 2] &&
            consonant(lemma[lemma_length - 1])) {
            lemma[--lemma_length] = '\0';
        } else if (lemma[lemma_length - 1] == 'v' ||
                   ends_with(lemma, lemma_length, "at") ||
                   ends_with(lemma, lemma_length, "iz")) {
            lemma[lemma_length++] = 'e';
            lemma[lemma_length] = '\0';
        }
        return lemma;
    }
    if (length > 4 &&
        (ends_with(word, length, "sses") || ends_with(word, length, "shes") ||
         ends_with(word, length, "ches") || ends_with(word, length, "xes") ||
         ends_with(word, length, "zes") || ends_with(word, length, "oes"))) {
        lemma[length - 2] = '\0';
        return lemma;
    }
    if (length > 3 && word[length - 1] == 's' &&
        !ends_with(word, length, "ss") && !ends_with(word, length, "us") &&
        !ends_with(word, length, "is")) {
        lemma[length - 1] = '\0';
    }
    return lemma;
}

static int compare_words(const void *left, const void *right) {
    const char *left_word = *static_cast<char *const *>(left);
    const char *right_word = *static_cast<char *const *>(right);
    return std::strcmp(left_word, right_word);
}

static FrequencyArray make_frequencies(WordArray *words) {
    std::qsort(words->items, words->size, sizeof(char *), compare_words);
    size_t unique_count = 0;
    size_t position = 0;
    while (position < words->size) {
        size_t next = position + 1;
        while (next < words->size &&
               std::strcmp(words->items[position], words->items[next]) == 0) {
            ++next;
        }
        ++unique_count;
        position = next;
    }
    FrequencyArray result = {nullptr, unique_count};
    if (unique_count != 0) {
        result.items = static_cast<TermFrequency *>(
            std::calloc(unique_count, sizeof(TermFrequency)));
        if (result.items == nullptr) {
            fail("out of memory");
        }
    }

    position = 0;
    size_t output = 0;
    while (position < words->size) {
        size_t next = position + 1;
        while (next < words->size &&
               std::strcmp(words->items[position], words->items[next]) == 0) {
            ++next;
        }
        result.items[output].term =
            duplicate_range(words->items[position],
                            std::strlen(words->items[position]));
        result.items[output].count = next - position;
        ++output;
        position = next;
    }
    return result;
}

static uint64_t elapsed_nanoseconds(const timespec &start, const timespec &end) {
    const uint64_t seconds = static_cast<uint64_t>(end.tv_sec - start.tv_sec);
    const int64_t nanoseconds = end.tv_nsec - start.tv_nsec;
    if (nanoseconds >= 0) {
        return seconds * 1'000'000'000ULL + static_cast<uint64_t>(nanoseconds);
    }
    return (seconds - 1) * 1'000'000'000ULL +
           static_cast<uint64_t>(nanoseconds + 1'000'000'000LL);
}

LinguisticResult linguistics_process(const char *text, size_t text_size,
                                     size_t maximum_length,
                                     const LemmaDictionary *dictionary) {
    timespec token_start = {};
    timespec token_end = {};
    timespec lemma_end = {};
    clock_gettime(CLOCK_MONOTONIC, &token_start);
    Buffer normalized = normalize_input(text, text_size);
    uint64_t token_characters = 0;
    WordArray tokens = tokenize(normalized.data, normalized.size, maximum_length,
                                &token_characters);
    std::free(normalized.data);
    clock_gettime(CLOCK_MONOTONIC, &token_end);

    WordArray lemmas = {nullptr, 0, 0};
    uint64_t changed = 0;
    for (size_t index = 0; index < tokens.size; ++index) {
        char *lemma = lemmatize(tokens.items[index], dictionary);
        if (std::strcmp(lemma, tokens.items[index]) != 0) {
            ++changed;
        }
        words_push(&lemmas, lemma);
    }
    clock_gettime(CLOCK_MONOTONIC, &lemma_end);

    LinguisticResult result = {};
    result.token_count = tokens.size;
    result.token_characters = token_characters;
    result.changed_token_count = changed;
    result.tokenization_nanoseconds =
        elapsed_nanoseconds(token_start, token_end);
    result.lemmatization_nanoseconds =
        elapsed_nanoseconds(token_end, lemma_end);
    result.tokens = make_frequencies(&tokens);
    result.lemmas = make_frequencies(&lemmas);

    words_free(&tokens);
    words_free(&lemmas);
    return result;
}

void linguistics_result_destroy(LinguisticResult *result) {
    for (size_t index = 0; index < result->tokens.size; ++index) {
        std::free(result->tokens.items[index].term);
    }
    for (size_t index = 0; index < result->lemmas.size; ++index) {
        std::free(result->lemmas.items[index].term);
    }
    std::free(result->tokens.items);
    std::free(result->lemmas.items);
    *result = {};
}

LemmaDictionary *lemma_dictionary_load(const char *path) {
    LemmaDictionary loaded = dictionary_load(path);
    LemmaDictionary *result = static_cast<LemmaDictionary *>(
        std::malloc(sizeof(LemmaDictionary)));
    if (result == nullptr) {
        dictionary_free(&loaded);
        fail("out of memory");
    }
    *result = loaded;
    return result;
}

void lemma_dictionary_destroy(LemmaDictionary *dictionary) {
    if (dictionary == nullptr) {
        return;
    }
    dictionary_free(dictionary);
    std::free(dictionary);
}
