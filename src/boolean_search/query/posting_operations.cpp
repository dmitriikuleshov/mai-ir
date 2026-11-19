// Реализует AND, OR и NOT как линейные операции над отсортированными posting lists.
#include "boolean_search/query/posting_operations.hpp"

#include <cmath>

namespace {

// Выбирает самый дальний сохранённый docID, не превосходящий цель второго списка.
size_t jump_towards(const std::vector<SkipPointer> &skips,
                    size_t current_position, uint32_t target_document,
                    size_t *skip_position) {
    while (*skip_position < skips.size() &&
           skips[*skip_position].posting_index <= current_position) {
        ++*skip_position;
    }
    size_t destination = current_position;
    while (*skip_position < skips.size() &&
           skips[*skip_position].document_id <= target_document) {
        destination = skips[*skip_position].posting_index;
        ++*skip_position;
    }
    return destination;
}

}  // namespace

// Добавляет равномерные указатели с шагом floor(sqrt(P)) к готовому списку.
BooleanPostingList make_posting_list(BooleanResult documents,
                                     size_t minimum_postings) {
    BooleanPostingList result{std::move(documents), {}};
    if (result.documents.size() < minimum_postings) return result;
    const size_t stride = static_cast<size_t>(
        std::sqrt(static_cast<double>(result.documents.size())));
    if (stride < 2) return result;
    result.skips.reserve((result.documents.size() - 1) / stride);
    for (size_t target = stride; target < result.documents.size();
         target += stride) {
        result.skips.push_back(
            {static_cast<uint32_t>(target), result.documents[target]});
    }
    return result;
}

// Возвращает пересечение, перепрыгивая заведомо слишком маленькие docID.
BooleanResult intersect_postings(const BooleanPostingList &left,
                                 const BooleanPostingList &right,
                                 bool use_skips, IntersectionStats *stats) {
    BooleanResult output;
    output.reserve(left.documents.size() < right.documents.size()
                       ? left.documents.size()
                       : right.documents.size());
    size_t first = 0;
    size_t second = 0;
    size_t first_skip = 0;
    size_t second_skip = 0;
    while (first < left.documents.size() &&
           second < right.documents.size()) {
        if (stats != nullptr) ++stats->comparisons;
        const uint32_t a = left.documents[first];
        const uint32_t b = right.documents[second];
        if (a == b) {
            output.push_back(a);
            ++first;
            ++second;
        } else if (a < b) {
            const size_t destination =
                use_skips ? jump_towards(left.skips, first, b, &first_skip)
                          : first;
            if (destination > first) {
                if (stats != nullptr) {
                    ++stats->skip_jumps;
                    stats->skipped_postings += destination - first;
                }
                first = destination;
            } else {
                ++first;
            }
        } else {
            const size_t destination =
                use_skips ? jump_towards(right.skips, second, a, &second_skip)
                          : second;
            if (destination > second) {
                if (stats != nullptr) {
                    ++stats->skip_jumps;
                    stats->skipped_postings += destination - second;
                }
                second = destination;
            } else {
                ++second;
            }
        }
    }
    return output;
}

// Базовый вариант нужен для сравнения результата и числа операций до ускорения.
BooleanResult intersect_postings(const BooleanResult &left,
                                 const BooleanResult &right) {
    return intersect_postings({left, {}}, {right, {}}, false, nullptr);
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
