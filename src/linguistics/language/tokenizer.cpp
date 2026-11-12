// Реализует нормализацию UTF-8 и разбиение технического текста на токены.
#include "linguistics/language/tokenizer.hpp"

#include <cctype>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace {

// CodePoint представляет символ Unicode, а Byte — отдельный байт его UTF-8-кодировки.
using CodePoint = char32_t;
using Byte = unsigned char;

struct UnicodeRange {
    CodePoint first;
    CodePoint last;
};

struct DecodedCharacter {
    CodePoint codepoint;
    size_t width;
    bool valid;
};

constexpr UnicodeRange GENERAL_PUNCTUATION{U'\u2000', U'\u206F'};
constexpr UnicodeRange SYMBOLS_AND_ARROWS{U'\u2190', U'\u2BFF'};
constexpr UnicodeRange CJK_PUNCTUATION{U'\u3000', U'\u303F'};
constexpr UnicodeRange TYPOGRAPHIC_SPACES{U'\u2000', U'\u200A'};
constexpr UnicodeRange FULLWIDTH_ASCII{U'\uFF01', U'\uFF5E'};

constexpr CodePoint DELETE_CHARACTER = U'\u007F';
constexpr CodePoint NO_BREAK_SPACE = U'\u00A0';
constexpr CodePoint LINE_SEPARATOR = U'\u2028';
constexpr CodePoint PARAGRAPH_SEPARATOR = U'\u2029';
constexpr CodePoint NARROW_NO_BREAK_SPACE = U'\u202F';
constexpr CodePoint MEDIUM_MATHEMATICAL_SPACE = U'\u205F';
constexpr CodePoint IDEOGRAPHIC_SPACE = U'\u3000';

constexpr CodePoint LEFT_SINGLE_QUOTE = U'\u2018';
constexpr CodePoint RIGHT_SINGLE_QUOTE = U'\u2019';
constexpr CodePoint HYPHEN = U'\u2010';
constexpr CodePoint NON_BREAKING_HYPHEN = U'\u2011';
constexpr CodePoint FIGURE_DASH = U'\u2012';
constexpr CodePoint EN_DASH = U'\u2013';
constexpr CodePoint EM_DASH = U'\u2014';

constexpr CodePoint ASCII_SPACE = U' ';
constexpr CodePoint FIRST_NON_ASCII = U'\u0080';
constexpr CodePoint FULLWIDTH_ASCII_OFFSET =
    FULLWIDTH_ASCII.first - U'!';

constexpr Byte ASCII_LIMIT = 0x80;

// Маски ниже отделяют служебные биты UTF-8 от битов самой кодовой точки.
constexpr Byte CONTINUATION_BYTE_MASK = 0xC0;
constexpr Byte CONTINUATION_BYTE_PREFIX = 0x80;
constexpr Byte CONTINUATION_DATA_MASK = 0x3F;
constexpr unsigned int CONTINUATION_DATA_BITS = 6;

constexpr Byte TWO_BYTE_PREFIX_MASK = 0xE0;
constexpr Byte TWO_BYTE_PREFIX = 0xC0;
constexpr Byte TWO_BYTE_DATA_MASK = 0x1F;
constexpr size_t TWO_BYTE_WIDTH = 2;

constexpr Byte THREE_BYTE_PREFIX_MASK = 0xF0;
constexpr Byte THREE_BYTE_PREFIX = 0xE0;
constexpr Byte THREE_BYTE_DATA_MASK = 0x0F;
constexpr size_t THREE_BYTE_WIDTH = 3;

constexpr Byte FOUR_BYTE_PREFIX_MASK = 0xF8;
constexpr Byte FOUR_BYTE_PREFIX = 0xF0;
constexpr Byte FOUR_BYTE_DATA_MASK = 0x07;
constexpr size_t FOUR_BYTE_WIDTH = 4;

constexpr CodePoint MINIMUM_THREE_BYTE_CODEPOINT =
    static_cast<CodePoint>(0x0800);
constexpr CodePoint MINIMUM_FOUR_BYTE_CODEPOINT =
    static_cast<CodePoint>(0x10000);
constexpr UnicodeRange SURROGATE_CODEPOINTS{
    static_cast<CodePoint>(0xD800), static_cast<CodePoint>(0xDFFF)};
constexpr CodePoint MAXIMUM_UNICODE_CODEPOINT =
    static_cast<CodePoint>(0x10FFFF);

// Проверяет, входит ли Unicode-символ в заданный диапазон кодовых точек.
constexpr bool belongs_to(CodePoint codepoint, UnicodeRange range) {
    return codepoint >= range.first && codepoint <= range.last;
}

// Определяет типографские варианты одинарной кавычки.
bool is_unicode_quote(CodePoint codepoint) {
    return codepoint == LEFT_SINGLE_QUOTE ||
           codepoint == RIGHT_SINGLE_QUOTE;
}

// Определяет поддерживаемые Unicode-варианты дефиса и тире.
bool is_unicode_dash(CodePoint codepoint) {
    return codepoint == HYPHEN || codepoint == NON_BREAKING_HYPHEN ||
           codepoint == FIGURE_DASH || codepoint == EN_DASH ||
           codepoint == EM_DASH;
}

// Проверяет, является ли байт продолжением многобайтового символа UTF-8.
bool is_continuation_byte(Byte byte) {
    return (byte & CONTINUATION_BYTE_MASK) == CONTINUATION_BYTE_PREFIX;
}

// Создаёт результат декодирования для некорректной последовательности UTF-8.
DecodedCharacter invalid_character() {
    // Один байт всё равно считается прочитанным, чтобы обработка продолжилась дальше.
    return {CodePoint{}, 1, false};
}

// Декодирует один символ UTF-8 и проверяет корректность его представления.
DecodedCharacter decode_utf8(const Byte *text, size_t remaining) {
    if (remaining == 0) return {CodePoint{}, 0, false};

    const Byte first = text[0];
    if (first < ASCII_LIMIT) {
        return {static_cast<CodePoint>(first), 1, true};
    }

    size_t width = 0;
    Byte leading_data_mask = 0;
    CodePoint minimum_codepoint = CodePoint{};
    if ((first & TWO_BYTE_PREFIX_MASK) == TWO_BYTE_PREFIX) {
        width = TWO_BYTE_WIDTH;
        leading_data_mask = TWO_BYTE_DATA_MASK;
        minimum_codepoint = FIRST_NON_ASCII;
    } else if ((first & THREE_BYTE_PREFIX_MASK) == THREE_BYTE_PREFIX) {
        width = THREE_BYTE_WIDTH;
        leading_data_mask = THREE_BYTE_DATA_MASK;
        minimum_codepoint = MINIMUM_THREE_BYTE_CODEPOINT;
    } else if ((first & FOUR_BYTE_PREFIX_MASK) == FOUR_BYTE_PREFIX) {
        width = FOUR_BYTE_WIDTH;
        leading_data_mask = FOUR_BYTE_DATA_MASK;
        minimum_codepoint = MINIMUM_FOUR_BYTE_CODEPOINT;
    } else {
        return invalid_character();
    }

    if (remaining < width) return invalid_character();

    CodePoint codepoint = static_cast<CodePoint>(first & leading_data_mask);
    for (size_t index = 1; index < width; ++index) {
        if (!is_continuation_byte(text[index])) return invalid_character();
        codepoint = static_cast<CodePoint>(
            (codepoint << CONTINUATION_DATA_BITS) |
            (text[index] & CONTINUATION_DATA_MASK));
    }

    // Избыточные кодировки, суррогаты UTF-16 и значения за пределами Unicode запрещены UTF-8.
    const bool overlong_encoding = codepoint < minimum_codepoint;
    const bool surrogate = belongs_to(codepoint, SURROGATE_CODEPOINTS);
    const bool outside_unicode = codepoint > MAXIMUM_UNICODE_CODEPOINT;
    if (overlong_encoding || surrogate || outside_unicode) {
        return invalid_character();
    }
    return {codepoint, width, true};
}

// Определяет Unicode-символы, которые разделяют соседние токены.
bool unicode_separator(CodePoint codepoint) {
    if (codepoint <= ASCII_SPACE || codepoint == DELETE_CHARACTER ||
        codepoint == NO_BREAK_SPACE) {
        return true;
    }
    return belongs_to(codepoint, GENERAL_PUNCTUATION) ||
           belongs_to(codepoint, SYMBOLS_AND_ARROWS) ||
           belongs_to(codepoint, CJK_PUNCTUATION);
}

// Определяет пробельные символы ASCII и Unicode.
bool unicode_space(CodePoint codepoint) {
    return codepoint <= ASCII_SPACE || codepoint == DELETE_CHARACTER ||
           codepoint == NO_BREAK_SPACE ||
           belongs_to(codepoint, TYPOGRAPHIC_SPACES) ||
           codepoint == LINE_SEPARATOR || codepoint == PARAGRAPH_SEPARATOR ||
           codepoint == NARROW_NO_BREAK_SPACE ||
           codepoint == MEDIUM_MATHEMATICAL_SPACE ||
           codepoint == IDEOGRAPHIC_SPACE;
}

// Приводит пробелы, кавычки, тире и fullwidth-символы к единому виду.
std::string normalize_input(const char *text, size_t text_size) {
    std::string normalized;
    normalized.reserve(text_size);
    size_t position = 0;
    while (position < text_size) {
        const DecodedCharacter character = decode_utf8(
            reinterpret_cast<const unsigned char *>(text + position),
            text_size - position);
        const CodePoint codepoint = character.codepoint;
        if (!character.valid) {
            normalized.push_back(' ');
        } else if (unicode_space(codepoint)) {
            normalized.push_back(' ');
        } else if (is_unicode_quote(codepoint)) {
            normalized.push_back('\'');
        } else if (is_unicode_dash(codepoint)) {
            normalized.push_back('-');
        } else if (belongs_to(codepoint, FULLWIDTH_ASCII)) {
            // Например, полноширинная латинская A превращается в обычную ASCII A.
            normalized.push_back(
                static_cast<char>(codepoint - FULLWIDTH_ASCII_OFFSET));
        } else {
            // Остальные корректные Unicode-символы копируются исходными UTF-8-байтами.
            normalized.append(text + position, character.width);
        }
        position += character.width;
    }
    return normalized;
}

// Проверяет, может ли следующий UTF-8-символ быть частью слова.
bool word_character(const unsigned char *text, size_t remaining,
                    size_t *width) {
    const DecodedCharacter character = decode_utf8(text, remaining);
    *width = character.width;
    if (!character.valid) return false;
    const CodePoint codepoint = character.codepoint;
    if (codepoint < FIRST_NON_ASCII) {
        return std::isalnum(static_cast<unsigned char>(codepoint)) != 0;
    }
    // Для не-ASCII символов токенизатор отделяет пунктуацию, но сохраняет буквы разных языков.
    return !unicode_separator(codepoint);
}

// Определяет допустимые разделители внутри составного технического токена.
bool inner_separator(unsigned char value) {
    return value == '.' || value == '_' || value == '\'' || value == '-';
}

// Проверяет, начинается ли слово с указанной позиции текста.
bool word_follows(const std::string &text, size_t position) {
    if (position >= text.size()) return false;
    size_t width = 1;
    return word_character(
        reinterpret_cast<const unsigned char *>(text.data() + position),
        text.size() - position, &width);
}

// Проверяет контекст, в котором разрешены префиксы --, / и @.
bool technical_prefix_can_start(const std::string &text, size_t position) {
    if (position == 0) return true;
    const unsigned char previous = text[position - 1];
    return std::isspace(previous) != 0 || previous == '(' || previous == '[' ||
           previous == '{' || previous == '<' || previous == ',' ||
           previous == ';' || previous == '=' || previous == '"' ||
           previous == '\'';
}

// Выделяет токены из нормализованного текста с учётом технических конструкций.
TokenList tokenize_normalized(const std::string &text, size_t maximum_length,
                              uint64_t *token_characters) {
    TokenList tokens;
    tokens.reserve(text.size() / 6 + 1);
    size_t position = 0;

    while (position < text.size()) {
        size_t width = 1;
        // Эти префиксы позволяют сохранить .NET, --option, /path и @package целиком.
        const bool starts_with_dot =
            text[position] == '.' && word_follows(text, position + 1);
        const bool starts_with_option =
            technical_prefix_can_start(text, position) &&
            text[position] == '-' && position + 1 < text.size() &&
            text[position + 1] == '-' && word_follows(text, position + 2);
        const bool starts_with_path =
            technical_prefix_can_start(text, position) &&
            text[position] == '/' && word_follows(text, position + 1);
        const bool starts_with_scope =
            technical_prefix_can_start(text, position) &&
            text[position] == '@' && word_follows(text, position + 1);
        if (!starts_with_dot && !starts_with_option && !starts_with_path &&
            !starts_with_scope &&
            !word_character(
                reinterpret_cast<const unsigned char *>(text.data() + position),
                text.size() - position, &width)) {
            position += width;
            continue;
        }

        std::string token;
        token.reserve(32);
        size_t characters = 0;
        bool option_token = false;
        bool path_token = false;
        bool scoped_token = false;
        bool url_token = false;
        if (starts_with_dot) {
            token.push_back('.');
            ++position;
            ++characters;
        } else if (starts_with_option) {
            token.append("--");
            position += 2;
            characters += 2;
            option_token = true;
        } else if (starts_with_path) {
            token.push_back('/');
            ++position;
            ++characters;
            path_token = true;
        } else if (starts_with_scope) {
            token.push_back('@');
            ++position;
            ++characters;
            scoped_token = true;
        }

        while (position < text.size()) {
            size_t character_width = 1;
            const bool is_word = word_character(
                reinterpret_cast<const unsigned char *>(text.data() + position),
                text.size() - position, &character_width);
            if (is_word) {
                if (character_width == 1) {
                    token.push_back(static_cast<char>(std::tolower(
                        static_cast<unsigned char>(text[position]))));
                } else {
                    token.append(text, position, character_width);
                }
                position += character_width;
                ++characters;
                continue;
            }

            if (character_width != 1) break;

            const unsigned char separator = text[position];
            // Внутренний разделитель сохраняется только между словами, но не на конце токена.
            if (inner_separator(separator) && word_follows(text, position + 1)) {
                token.push_back(text[position++]);
                ++characters;
                continue;
            }
            if (separator == ':' && position + 2 < text.size() &&
                text[position + 1] == '/' && text[position + 2] == '/' &&
                word_follows(text, position + 3)) {
                token.append("://");
                position += 3;
                characters += 3;
                // Дополнительные URL-разделители разрешаются только после распознанной схемы.
                url_token = true;
                continue;
            }
            if (separator == ':' && position + 1 < text.size() &&
                text[position + 1] == ':' && word_follows(text, position + 2)) {
                token.append("::");
                position += 2;
                characters += 2;
                continue;
            }
            if (separator == '@' && word_follows(text, position + 1)) {
                token.push_back('@');
                ++position;
                ++characters;
                continue;
            }
            if (separator == '/' &&
                (path_token || scoped_token || url_token) &&
                word_follows(text, position + 1)) {
                token.push_back('/');
                ++position;
                ++characters;
                continue;
            }
            if (option_token && separator == '=' &&
                word_follows(text, position + 1)) {
                token.push_back('=');
                ++position;
                ++characters;
                continue;
            }
            const bool url_separator =
                separator == ':' || separator == '?' || separator == '&' ||
                separator == '=' || separator == '%' || separator == '#' ||
                separator == '+';
            if (url_token && url_separator &&
                word_follows(text, position + 1)) {
                token.push_back(static_cast<char>(separator));
                ++position;
                ++characters;
                continue;
            }
            break;
        }

        // Суффиксы языков C++ и C# добавляются отдельно, поскольку сами по себе не являются словом.
        if (position + 1 < text.size() && text[position] == '+' &&
            text[position + 1] == '+') {
            token.append("++");
            position += 2;
            characters += 2;
        } else if (position < text.size() && text[position] == '#') {
            token.push_back('#');
            ++position;
            ++characters;
        }

        // Английское притяжательное окончание удаляется до передачи токена лемматизатору.
        if (token.size() >= 2 && token.ends_with("'s")) {
            token.resize(token.size() - 2);
            characters -= 2;
        }
        // Ограничение задаётся в Unicode-символах, а не в количестве UTF-8-байтов.
        if (!token.empty() && characters <= maximum_length) {
            *token_characters += characters;
            tokens.push_back(std::move(token));
        }
    }
    return tokens;
}

}  // namespace

// Нормализует исходный текст, выполняет токенизацию и измеряет время обработки.
TokenizationResult tokenize_text(const char *text, size_t text_size,
                                 size_t maximum_token_length) {
    const auto started = std::chrono::steady_clock::now();

    const std::string normalized = normalize_input(text, text_size);
    TokenizationResult result;
    result.tokens = tokenize_normalized(
        normalized, maximum_token_length, &result.token_characters);

    const auto finished = std::chrono::steady_clock::now();
    result.elapsed_nanoseconds = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started)
            .count());
    return result;
}
