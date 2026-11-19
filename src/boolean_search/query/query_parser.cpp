// Разбирает выражения с AND, OR, NOT и скобками и сразу вычисляет их posting lists.
#include "boolean_search/query/query_parser.hpp"

#include "boolean_search/index/index_reader.hpp"
#include "boolean_search/query/posting_operations.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace {

// Типы лексем отделяют поисковые термы от операторов и скобок.
enum class TokenType { END, TERM, AND, OR, NOT, LEFT, RIGHT, INVALID };

// Одна лексема запроса; текст требуется только для поискового терма.
struct Token {
    TokenType type;
    std::string text;
};

// Хранит состояние рекурсивного спуска и ресурсы, необходимые для вычисления запроса.
struct Parser {
    BooleanIndex *index;
    const char *cursor;
    size_t maximum_token_length;
    const LemmaDictionary *dictionary;
    bool use_skip_pointers;
    IntersectionStats *stats;
    Token current;
    char *error;
    size_t error_size;
};

// Сохраняет первую понятную пользователю ошибку синтаксиса в выходном буфере.
void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

// Сравнивает слово с оператором без учёта регистра ASCII.
bool word_equals(const char *word, size_t size, const char *expected) {
    const size_t expected_size = std::strlen(expected);
    if (size != expected_size) {
        return false;
    }
    for (size_t index = 0; index < size; ++index) {
        if (std::toupper(static_cast<unsigned char>(word[index])) !=
            expected[index]) {
            return false;
        }
    }
    return true;
}

// Выделяет следующую лексему, считая скобки самостоятельными разделителями.
Token next_token(Parser *parser) {
    while (*parser->cursor != '\0' &&
           std::isspace(static_cast<unsigned char>(*parser->cursor)) != 0) {
        ++parser->cursor;
    }
    if (*parser->cursor == '\0') {
        return {TokenType::END, {}};
    }
    if (*parser->cursor == '(') {
        ++parser->cursor;
        return {TokenType::LEFT, {}};
    }
    if (*parser->cursor == ')') {
        ++parser->cursor;
        return {TokenType::RIGHT, {}};
    }
    const char *begin = parser->cursor;
    while (*parser->cursor != '\0' && *parser->cursor != '(' &&
           *parser->cursor != ')' &&
           std::isspace(static_cast<unsigned char>(*parser->cursor)) == 0) {
        ++parser->cursor;
    }
    const size_t size = static_cast<size_t>(parser->cursor - begin);
    if (word_equals(begin, size, "AND")) {
        return {TokenType::AND, {}};
    }
    if (word_equals(begin, size, "OR")) {
        return {TokenType::OR, {}};
    }
    if (word_equals(begin, size, "NOT")) {
        return {TokenType::NOT, {}};
    }
    return {TokenType::TERM, std::string(begin, size)};
}

// Переводит парсер к следующей лексеме запроса.
void advance(Parser *parser) {
    parser->current = next_token(parser);
}

// Предварительное объявление нижнего по приоритету уровня OR для разбора скобок.
BooleanPostingList parse_or(Parser *parser, bool *ok);

// Разбирает один терм или выражение в скобках.
BooleanPostingList parse_primary(Parser *parser, bool *ok) {
    if (parser->current.type == TokenType::LEFT) {
        advance(parser);
        BooleanPostingList result = parse_or(parser, ok);
        if (*ok && parser->current.type != TokenType::RIGHT) {
            set_error(parser->error, parser->error_size,
                      "ожидалась закрывающая скобка");
            *ok = false;
        }
        if (*ok) {
            advance(parser);
        }
        return result;
    }
    if (parser->current.type != TokenType::TERM) {
        set_error(parser->error, parser->error_size,
                  "ожидался поисковый термин");
        *ok = false;
        return {};
    }
    LinguisticResult normalized = linguistics_process(
        parser->current.text.data(), parser->current.text.size(),
        parser->maximum_token_length, parser->dictionary);
    BooleanPostingList result{};
    // Документы и запрос нормализуются одним pipeline; терм должен дать ровно одну лемму.
    if (normalized.lemmas.size() == 1) {
        result = load_boolean_postings(
            parser->index, normalized.lemmas[0].term.c_str());
        if (!parser->use_skip_pointers) result.skips.clear();
    } else {
        set_error(parser->error, parser->error_size,
                  "термин не удалось однозначно нормализовать");
        *ok = false;
    }
    advance(parser);
    return result;
}

// Разбирает унарный NOT; рекурсивный вызов поддерживает цепочки вроде NOT NOT term.
BooleanPostingList parse_unary(Parser *parser, bool *ok) {
    if (parser->current.type == TokenType::NOT) {
        advance(parser);
        BooleanPostingList operand = parse_unary(parser, ok);
        if (!*ok) {
            return operand;
        }
        return make_posting_list(
            complement_postings(operand.documents,
                                parser->index->header.document_count),
            16);
    }
    return parse_primary(parser, ok);
}

// Разбирает последовательность AND, имеющую приоритет выше OR.
BooleanPostingList parse_and(Parser *parser, bool *ok) {
    BooleanPostingList left = parse_unary(parser, ok);
    while (*ok && parser->current.type == TokenType::AND) {
        advance(parser);
        BooleanPostingList right = parse_unary(parser, ok);
        if (!*ok) {
            break;
        }
        left = make_posting_list(
            intersect_postings(left, right, parser->use_skip_pointers,
                               parser->stats),
            16);
    }
    return left;
}

// Разбирает последовательность OR как самый низкий уровень приоритета.
BooleanPostingList parse_or(Parser *parser, bool *ok) {
    BooleanPostingList left = parse_and(parser, ok);
    while (*ok && parser->current.type == TokenType::OR) {
        advance(parser);
        BooleanPostingList right = parse_and(parser, ok);
        if (!*ok) {
            break;
        }
        left = make_posting_list(
            unite_postings(left.documents, right.documents), 16);
    }
    return left;
}

}  // namespace

// Проверяет запрос, запускает рекурсивный спуск и возвращает отсортированные docID.
BooleanResult boolean_search(BooleanIndex *index, const char *query,
                             size_t maximum_token_length,
                             const LemmaDictionary *dictionary,
                             bool use_skip_pointers,
                             IntersectionStats *stats,
                             char *error, size_t error_size) {
    if (error != nullptr && error_size != 0) {
        error[0] = '\0';
    }
    if (stats != nullptr) *stats = {};
    Parser parser{index, query, maximum_token_length, dictionary,
                  use_skip_pointers, stats, {TokenType::END, {}}, error,
                  error_size};
    // Цепочка parse_or → parse_and → parse_unary → parse_primary задаёт приоритеты.
    parser.current = next_token(&parser);
    if (parser.current.type == TokenType::END) {
        set_error(error, error_size, "пустой запрос");
        return {};
    }
    bool ok = parser.current.type != TokenType::INVALID;
    BooleanPostingList result = parse_or(&parser, &ok);
    if (ok && parser.current.type != TokenType::END) {
        set_error(error, error_size,
                  "после выражения остались лишние данные");
        ok = false;
    }
    return ok ? std::move(result.documents) : BooleanResult{};
}
