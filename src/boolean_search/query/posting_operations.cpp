// Реализует AND, OR и NOT как линейные операции над отсортированными posting lists.
#include "boolean_search/query/posting_operations.hpp"

// Возвращает пересечение двух списков документов для операции AND.
BooleanResult intersect_postings(const BooleanResult &left,
                                 const BooleanResult &right) {
    BooleanResult output;
    output.reserve(left.size() < right.size() ? left.size() : right.size());
    size_t first = 0;
    size_t second = 0;
    // Меньший docID продвигается вперёд, поэтому каждый список читается один раз.
    while (first < left.size() && second < right.size()) {
        const uint32_t a = left[first];
        const uint32_t b = right[second];
        if (a == b) {
            output.push_back(a);
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

// Возвращает объединение двух списков без повторов для операции OR.
BooleanResult unite_postings(const BooleanResult &left,
                             const BooleanResult &right) {
    BooleanResult output;
    output.reserve(left.size() + right.size());
    size_t first = 0;
    size_t second = 0;
    while (first < left.size() || second < right.size()) {
        uint32_t value = 0;
        if (second == right.size() ||
            (first < left.size() && left[first] < right[second])) {
            value = left[first++];
        } else if (first == left.size() || right[second] < left[first]) {
            value = right[second++];
        } else {
            value = left[first];
            ++first;
            ++second;
        }
        output.push_back(value);
    }
    return output;
}

// Возвращает все docID корпуса, отсутствующие во входном списке, для операции NOT.
BooleanResult complement_postings(const BooleanResult &input,
                                  uint32_t document_count) {
    BooleanResult output;
    output.reserve(static_cast<size_t>(document_count) - input.size());
    size_t position = 0;
    for (uint32_t document = 0; document < document_count; ++document) {
        if (position < input.size() && input[position] == document) {
            ++position;
        } else {
            output.push_back(document);
        }
    }
    return output;
}
