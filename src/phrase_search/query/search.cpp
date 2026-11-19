// Нормализует цитату и оставляет документы с последовательными координатами.
#include "phrase_search/query/search.hpp"

#include "linguistics/language/pipeline.hpp"
#include "phrase_search/index/index_reader.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace {

void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

std::string clean_query(const char *query) {
    std::string value(query == nullptr ? "" : query);
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const size_t end = value.find_last_not_of(" \t\r\n");
    value = value.substr(begin, end - begin + 1);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

// Возвращает позиции правого терма, стоящие ровно после позиции левого.
std::vector<uint32_t> adjacent_positions(
    const std::vector<uint32_t> &left,
    const std::vector<uint32_t> &right) {
    std::vector<uint32_t> result;
    size_t first = 0;
    size_t second = 0;
    while (first < left.size() && second < right.size()) {
        if (left[first] == UINT32_MAX) {
            ++first;
            continue;
        }
        const uint32_t expected = left[first] + 1;
        if (right[second] == expected) {
            result.push_back(right[second]);
            ++first;
            ++second;
        } else if (right[second] < expected) {
            ++second;
        } else {
            ++first;
        }
    }
    return result;
}

PositionPostingList intersect_adjacent(const PositionPostingList &left,
                                       const PositionPostingList &right) {
    PositionPostingList result;
    size_t first = 0;
    size_t second = 0;
    while (first < left.size() && second < right.size()) {
        if (left[first].document_id < right[second].document_id) {
            ++first;
        } else if (right[second].document_id < left[first].document_id) {
            ++second;
        } else {
            std::vector<uint32_t> positions = adjacent_positions(
                left[first].positions, right[second].positions);
            if (!positions.empty()) {
                result.push_back(
                    {left[first].document_id, std::move(positions)});
            }
            ++first;
            ++second;
        }
    }
    return result;
}

}  // namespace

PhraseSearchResult intersect_phrase_positions(
    const std::vector<PositionPostingList> &terms) {
    PhraseSearchResult result{};
    if (terms.empty() || terms.front().empty()) return result;
    PositionPostingList current = terms.front();
    for (size_t term_index = 1; term_index < terms.size(); ++term_index) {
        current = intersect_adjacent(current, terms[term_index]);
        if (current.empty()) return result;
    }

    const uint32_t offset = static_cast<uint32_t>(terms.size() - 1);
    result.documents.reserve(current.size());
    for (PositionPosting &posting : current) {
        for (uint32_t &position : posting.positions) {
            if (position < offset) return {};
            position -= offset;
        }
        result.occurrence_count += posting.positions.size();
        result.documents.push_back(
            {posting.document_id, std::move(posting.positions)});
    }
    return result;
}

PhraseSearchResult phrase_search(PositionalIndex *index, const char *query,
                                 size_t maximum_token_length,
                                 const LemmaDictionary *dictionary,
                                 char *error, size_t error_size) {
    if (error != nullptr && error_size != 0) error[0] = '\0';
    if (index == nullptr || query == nullptr || dictionary == nullptr) {
        set_error(error, error_size, "не переданы индекс, запрос или словарь");
        return {};
    }
    const std::string cleaned = clean_query(query);
    if (cleaned.empty()) {
        set_error(error, error_size, "пустая цитата");
        return {};
    }
    const TokenList lemmas = linguistics_normalize_sequence(
        cleaned.data(), cleaned.size(), maximum_token_length, dictionary);
    if (lemmas.empty()) {
        set_error(error, error_size,
                  "цитата не содержит индексируемых терминов");
        return {};
    }
    if (lemmas.size() > UINT32_MAX) {
        set_error(error, error_size, "цитата слишком длинная");
        return {};
    }

    std::vector<PositionPostingList> postings;
    postings.reserve(lemmas.size());
    for (const std::string &lemma : lemmas) {
        PositionPostingList list;
        bool found = false;
        if (!load_position_postings(index, lemma.c_str(), &list, &found)) {
            set_error(error, error_size,
                      "не удалось прочитать координаты из индекса");
            return {};
        }
        if (!found) return {};
        postings.push_back(std::move(list));
    }
    return intersect_phrase_positions(postings);
}
