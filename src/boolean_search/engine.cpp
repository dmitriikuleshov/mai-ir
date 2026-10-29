#include "engine.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using boolean_index_format::DocumentHeader;
using boolean_index_format::INDEX_MAGIC;
using boolean_index_format::LexiconEntry;

namespace {

enum class TokenType { END, TERM, AND, OR, NOT, LEFT, RIGHT, INVALID };

struct Token {
    TokenType type;
    char *text;
};

struct Parser {
    BooleanIndex *index;
    const char *cursor;
    size_t maximum_token_length;
    const LemmaDictionary *dictionary;
    Token current;
    char *error;
    size_t error_size;
};

FILE *as_file(void *pointer) {
    return static_cast<FILE *>(pointer);
}

void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

char *make_path(const char *directory, const char *name) {
    const size_t size = std::strlen(directory) + std::strlen(name) + 2;
    char *path = static_cast<char *>(std::malloc(size));
    if (path != nullptr) {
        std::snprintf(path, size, "%s/%s", directory, name);
    }
    return path;
}

bool read_exact(FILE *file, void *target, size_t size) {
    return std::fread(target, 1, size, file) == size;
}

bool seek_file(FILE *file, uint64_t offset) {
    return offset <= static_cast<uint64_t>(INT64_MAX) &&
           fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
}

bool compare_term(BooleanIndex *index, const LexiconEntry *entry,
                  const char *term, char **buffer, size_t *capacity,
                  int *comparison) {
    const size_t needed = static_cast<size_t>(entry->term_length) + 1;
    if (*capacity < needed) {
        char *grown = static_cast<char *>(std::realloc(*buffer, needed));
        if (grown == nullptr) {
            return false;
        }
        *buffer = grown;
        *capacity = needed;
    }
    FILE *terms = as_file(index->terms_file);
    if (!seek_file(terms, entry->term_offset) ||
        !read_exact(terms, *buffer, entry->term_length)) {
        return false;
    }
    (*buffer)[entry->term_length] = '\0';
    *comparison = std::strcmp(*buffer, term);
    return true;
}

bool find_term(BooleanIndex *index, const char *term, LexiconEntry *result) {
    uint64_t left = 0;
    uint64_t right = index->header.term_count;
    char *buffer = nullptr;
    size_t capacity = 0;
    FILE *lexicon = as_file(index->lexicon_file);
    while (left < right) {
        const uint64_t middle = left + (right - left) / 2;
        const uint64_t offset = sizeof(boolean_index_format::IndexHeader) +
                                middle * sizeof(LexiconEntry);
        LexiconEntry entry{};
        if (!seek_file(lexicon, offset) || !read_exact(lexicon, &entry, sizeof(entry))) {
            std::free(buffer);
            return false;
        }
        int comparison = 0;
        if (!compare_term(index, &entry, term, &buffer, &capacity,
                          &comparison)) {
            std::free(buffer);
            return false;
        }
        if (comparison == 0) {
            *result = entry;
            std::free(buffer);
            return true;
        }
        if (comparison < 0) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    std::free(buffer);
    return false;
}

BooleanResult empty_result() {
    return {nullptr, 0};
}

BooleanResult load_postings(BooleanIndex *index, const char *term) {
    LexiconEntry entry{};
    if (!find_term(index, term, &entry) || entry.postings_count == 0) {
        return empty_result();
    }
    BooleanResult result{};
    result.size = entry.postings_count;
    result.document_ids = static_cast<uint32_t *>(
        std::malloc(result.size * sizeof(uint32_t)));
    FILE *postings = as_file(index->postings_file);
    if (result.document_ids == nullptr ||
        !seek_file(postings, entry.postings_offset) ||
        !read_exact(postings, result.document_ids,
                    result.size * sizeof(uint32_t))) {
        boolean_result_destroy(&result);
    }
    return result;
}

BooleanResult result_and(const BooleanResult *left, const BooleanResult *right) {
    BooleanResult output{};
    const size_t capacity = left->size < right->size ? left->size : right->size;
    if (capacity != 0) {
        output.document_ids = static_cast<uint32_t *>(
            std::malloc(capacity * sizeof(uint32_t)));
        if (output.document_ids == nullptr) {
            return empty_result();
        }
    }
    size_t first = 0;
    size_t second = 0;
    while (first < left->size && second < right->size) {
        const uint32_t a = left->document_ids[first];
        const uint32_t b = right->document_ids[second];
        if (a == b) {
            output.document_ids[output.size++] = a;
            ++first;
            ++second;
        } else if (a < b) {
            ++first;
        } else {
            ++second;
        }
    }
    return output;
}

BooleanResult result_or(const BooleanResult *left, const BooleanResult *right) {
    BooleanResult output{};
    const size_t capacity = left->size + right->size;
    if (capacity != 0) {
        output.document_ids = static_cast<uint32_t *>(
            std::malloc(capacity * sizeof(uint32_t)));
        if (output.document_ids == nullptr) {
            return empty_result();
        }
    }
    size_t first = 0;
    size_t second = 0;
    while (first < left->size || second < right->size) {
        uint32_t value = 0;
        if (second == right->size ||
            (first < left->size && left->document_ids[first] < right->document_ids[second])) {
            value = left->document_ids[first++];
        } else if (first == left->size || right->document_ids[second] < left->document_ids[first]) {
            value = right->document_ids[second++];
        } else {
            value = left->document_ids[first];
            ++first;
            ++second;
        }
        output.document_ids[output.size++] = value;
    }
    return output;
}

BooleanResult result_not(const BooleanResult *input, uint32_t document_count) {
    BooleanResult output{};
    const size_t capacity = static_cast<size_t>(document_count) - input->size;
    if (capacity != 0) {
        output.document_ids = static_cast<uint32_t *>(
            std::malloc(capacity * sizeof(uint32_t)));
        if (output.document_ids == nullptr) {
            return empty_result();
        }
    }
    size_t position = 0;
    for (uint32_t document = 0; document < document_count; ++document) {
        if (position < input->size && input->document_ids[position] == document) {
            ++position;
        } else {
            output.document_ids[output.size++] = document;
        }
    }
    return output;
}

bool word_equals(const char *word, size_t size, const char *expected) {
    const size_t expected_size = std::strlen(expected);
    if (size != expected_size) {
        return false;
    }
    for (size_t index = 0; index < size; ++index) {
        if (std::toupper(static_cast<unsigned char>(word[index])) != expected[index]) {
            return false;
        }
    }
    return true;
}

Token next_token(Parser *parser) {
    while (*parser->cursor != '\0' &&
           std::isspace(static_cast<unsigned char>(*parser->cursor)) != 0) {
        ++parser->cursor;
    }
    if (*parser->cursor == '\0') {
        return {TokenType::END, nullptr};
    }
    if (*parser->cursor == '(') {
        ++parser->cursor;
        return {TokenType::LEFT, nullptr};
    }
    if (*parser->cursor == ')') {
        ++parser->cursor;
        return {TokenType::RIGHT, nullptr};
    }
    const char *begin = parser->cursor;
    while (*parser->cursor != '\0' && *parser->cursor != '(' &&
           *parser->cursor != ')' &&
           std::isspace(static_cast<unsigned char>(*parser->cursor)) == 0) {
        ++parser->cursor;
    }
    const size_t size = static_cast<size_t>(parser->cursor - begin);
    if (word_equals(begin, size, "AND")) {
        return {TokenType::AND, nullptr};
    }
    if (word_equals(begin, size, "OR")) {
        return {TokenType::OR, nullptr};
    }
    if (word_equals(begin, size, "NOT")) {
        return {TokenType::NOT, nullptr};
    }
    char *text = static_cast<char *>(std::malloc(size + 1));
    if (text == nullptr) {
        return {TokenType::INVALID, nullptr};
    }
    std::memcpy(text, begin, size);
    text[size] = '\0';
    return {TokenType::TERM, text};
}

void advance(Parser *parser) {
    std::free(parser->current.text);
    parser->current = next_token(parser);
}

BooleanResult parse_or(Parser *parser, bool *ok);

BooleanResult parse_primary(Parser *parser, bool *ok) {
    if (parser->current.type == TokenType::LEFT) {
        advance(parser);
        BooleanResult result = parse_or(parser, ok);
        if (*ok && parser->current.type != TokenType::RIGHT) {
            set_error(parser->error, parser->error_size, "ожидалась закрывающая скобка");
            *ok = false;
        }
        if (*ok) {
            advance(parser);
        }
        return result;
    }
    if (parser->current.type != TokenType::TERM) {
        set_error(parser->error, parser->error_size, "ожидался поисковый термин");
        *ok = false;
        return empty_result();
    }
    LinguisticResult normalized = linguistics_process(
        parser->current.text, std::strlen(parser->current.text),
        parser->maximum_token_length, parser->dictionary);
    BooleanResult result{};
    if (normalized.lemmas.size == 1) {
        result = load_postings(parser->index, normalized.lemmas.items[0].term);
    } else {
        set_error(parser->error, parser->error_size,
                  "термин не удалось однозначно нормализовать");
        *ok = false;
    }
    linguistics_result_destroy(&normalized);
    advance(parser);
    return result;
}

BooleanResult parse_unary(Parser *parser, bool *ok) {
    if (parser->current.type == TokenType::NOT) {
        advance(parser);
        BooleanResult operand = parse_unary(parser, ok);
        if (!*ok) {
            return operand;
        }
        BooleanResult output = result_not(&operand, parser->index->header.document_count);
        boolean_result_destroy(&operand);
        return output;
    }
    return parse_primary(parser, ok);
}

BooleanResult parse_and(Parser *parser, bool *ok) {
    BooleanResult left = parse_unary(parser, ok);
    while (*ok && parser->current.type == TokenType::AND) {
        advance(parser);
        BooleanResult right = parse_unary(parser, ok);
        if (!*ok) {
            boolean_result_destroy(&right);
            break;
        }
        BooleanResult merged = result_and(&left, &right);
        boolean_result_destroy(&left);
        boolean_result_destroy(&right);
        left = merged;
    }
    return left;
}

BooleanResult parse_or(Parser *parser, bool *ok) {
    BooleanResult left = parse_and(parser, ok);
    while (*ok && parser->current.type == TokenType::OR) {
        advance(parser);
        BooleanResult right = parse_and(parser, ok);
        if (!*ok) {
            boolean_result_destroy(&right);
            break;
        }
        BooleanResult merged = result_or(&left, &right);
        boolean_result_destroy(&left);
        boolean_result_destroy(&right);
        left = merged;
    }
    return left;
}

}  // namespace

bool boolean_index_open(BooleanIndex *index, const char *directory,
                        char *error, size_t error_size) {
    *index = {};
    const char *names[] = {"lexicon.bin", "terms.bin", "postings.bin", "documents.bin"};
    void **targets[] = {&index->lexicon_file, &index->terms_file,
                        &index->postings_file, &index->documents_file};
    for (size_t item = 0; item < 4; ++item) {
        char *path = make_path(directory, names[item]);
        FILE *file = path == nullptr ? nullptr : std::fopen(path, "rb");
        std::free(path);
        if (file == nullptr) {
            set_error(error, error_size, "не удалось открыть файлы индекса");
            boolean_index_close(index);
            return false;
        }
        *targets[item] = file;
    }
    if (!read_exact(as_file(index->lexicon_file), &index->header,
                    sizeof(index->header)) ||
        std::memcmp(index->header.magic, INDEX_MAGIC, sizeof(INDEX_MAGIC)) != 0 ||
        index->header.version != boolean_index_format::VERSION) {
        set_error(error, error_size, "неверный формат или версия индекса");
        boolean_index_close(index);
        return false;
    }
    DocumentHeader documents{};
    if (!read_exact(as_file(index->documents_file), &documents, sizeof(documents)) ||
        std::memcmp(documents.magic, boolean_index_format::DOCUMENT_MAGIC,
                    sizeof(documents.magic)) != 0 ||
        documents.version != boolean_index_format::VERSION ||
        documents.document_count != index->header.document_count) {
        set_error(error, error_size, "таблица документов не соответствует индексу");
        boolean_index_close(index);
        return false;
    }
    return true;
}

void boolean_index_close(BooleanIndex *index) {
    void **files[] = {&index->lexicon_file, &index->terms_file,
                      &index->postings_file, &index->documents_file};
    for (void **file : files) {
        if (*file != nullptr) {
            std::fclose(as_file(*file));
            *file = nullptr;
        }
    }
    index->header = {};
}

bool boolean_index_document_oid(BooleanIndex *index, uint32_t document_id,
                                uint8_t oid[12]) {
    if (document_id >= index->header.document_count) {
        return false;
    }
    FILE *documents = as_file(index->documents_file);
    const uint64_t offset = sizeof(DocumentHeader) +
                            static_cast<uint64_t>(document_id) * 12;
    return seek_file(documents, offset) && read_exact(documents, oid, 12);
}

BooleanResult boolean_search(BooleanIndex *index, const char *query,
                             size_t maximum_token_length,
                             const LemmaDictionary *dictionary,
                             char *error, size_t error_size) {
    if (error != nullptr && error_size != 0) {
        error[0] = '\0';
    }
    Parser parser{index, query, maximum_token_length, dictionary,
                  {TokenType::END, nullptr}, error, error_size};
    parser.current = next_token(&parser);
    if (parser.current.type == TokenType::END) {
        set_error(error, error_size, "пустой запрос");
        return empty_result();
    }
    bool ok = parser.current.type != TokenType::INVALID;
    BooleanResult result = parse_or(&parser, &ok);
    if (ok && parser.current.type != TokenType::END) {
        set_error(error, error_size, "после выражения остались лишние данные");
        ok = false;
    }
    std::free(parser.current.text);
    if (!ok) {
        boolean_result_destroy(&result);
        return empty_result();
    }
    return result;
}

void boolean_result_destroy(BooleanResult *result) {
    std::free(result->document_ids);
    *result = {};
}
