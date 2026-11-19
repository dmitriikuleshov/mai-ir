#include <gtest/gtest.h>

#include "boolean_search/query/posting_operations.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t DOCUMENT_COUNT = 200000;
constexpr size_t MEASUREMENT_REPETITIONS = 11;

struct MeasuredSearch {
    BooleanResult documents;
    IntersectionStats stats;
    double median_microseconds = 0.0;
};

struct SearchComparison {
    MeasuredSearch linear;
    MeasuredSearch with_skips;
};

// Создаёт плотный posting list с docID от нуля до count - 1.
BooleanResult make_sequence(uint32_t count) {
    BooleanResult result;
    result.reserve(count);
    for (uint32_t document = 0; document < count; ++document) {
        result.push_back(document);
    }
    return result;
}

// Создаёт posting list из docID, расположенных через заданный шаг.
BooleanResult make_sparse(uint32_t count, uint32_t step) {
    BooleanResult result;
    result.reserve(count / step + 1);
    for (uint32_t document = 0; document < count; document += step) {
        result.push_back(document);
    }
    if (result.back() != count - 1) result.push_back(count - 1);
    return result;
}

// Возвращает медиану времени, снижая влияние случайных выбросов.
double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

// Один раз выполняет AND, замеряя время и собирая счётчики алгоритма.
double measure_once(const BooleanPostingList &left,
                    const BooleanPostingList &right, bool use_skips,
                    BooleanResult *documents, IntersectionStats *stats) {
    *stats = {};
    const auto started = std::chrono::steady_clock::now();
    *documents = intersect_postings(left, right, use_skips, stats);
    return std::chrono::duration<double, std::micro>(
               std::chrono::steady_clock::now() - started)
        .count();
}

// Чередует порядок запусков и сравнивает медианное время двух алгоритмов.
SearchComparison measure_search(const BooleanPostingList &left,
                                const BooleanPostingList &right) {
    SearchComparison comparison;
    std::vector<double> linear_times;
    std::vector<double> skip_times;
    linear_times.reserve(MEASUREMENT_REPETITIONS);
    skip_times.reserve(MEASUREMENT_REPETITIONS);

    // Первые запуски прогревают код и память и не входят в измерение.
    (void)intersect_postings(left, right, false, nullptr);
    (void)intersect_postings(left, right, true, nullptr);

    for (size_t repetition = 0; repetition < MEASUREMENT_REPETITIONS;
         ++repetition) {
        auto measure_linear = [&] {
            linear_times.push_back(measure_once(
                left, right, false, &comparison.linear.documents,
                &comparison.linear.stats));
        };
        auto measure_skips = [&] {
            skip_times.push_back(measure_once(
                left, right, true, &comparison.with_skips.documents,
                &comparison.with_skips.stats));
        };
        if (repetition % 2 == 0) {
            measure_linear();
            measure_skips();
        } else {
            measure_skips();
            measure_linear();
        }
    }

    comparison.linear.median_microseconds = median(std::move(linear_times));
    comparison.with_skips.median_microseconds = median(std::move(skip_times));
    return comparison;
}

// Печатает компактное сравнение времени и числа операций.
void print_comparison(const char *scenario,
                      const SearchComparison &comparison) {
    const auto linear_comparisons = comparison.linear.stats.comparisons;
    const auto skip_comparisons = comparison.with_skips.stats.comparisons;
    const double saved_percent =
        linear_comparisons == 0
            ? 0.0
            : 100.0 * (static_cast<double>(linear_comparisons) -
                       static_cast<double>(skip_comparisons)) /
                  static_cast<double>(linear_comparisons);

    std::printf("\n%s\n", scenario);
    std::printf("  Время:      %.3f -> %.3f мкс\n",
                comparison.linear.median_microseconds,
                comparison.with_skips.median_microseconds);
    std::printf("  Сравнения: %llu -> %llu (-%.2f %%)\n",
                static_cast<unsigned long long>(linear_comparisons),
                static_cast<unsigned long long>(skip_comparisons),
                saved_percent);
    std::printf("  Прыжки:     %llu, заменено шагов: %llu\n",
                static_cast<unsigned long long>(
                    comparison.with_skips.stats.skip_jumps),
                static_cast<unsigned long long>(
                    comparison.with_skips.stats.skipped_postings));
}

// Проверяется максимальная польза прыжков при совпадении в конце списка.
TEST(BooleanSearchComparisonTest, DistantDocument) {
    const BooleanPostingList left =
        make_posting_list(make_sequence(DOCUMENT_COUNT), 16);
    const BooleanPostingList right{{DOCUMENT_COUNT - 1}, {}};

    const SearchComparison comparison = measure_search(left, right);
    print_comparison("1. Совпадение в конце длинного списка", comparison);

    EXPECT_EQ(comparison.with_skips.documents, comparison.linear.documents);
    ASSERT_EQ(comparison.linear.documents, BooleanResult({DOCUMENT_COUNT - 1}));
    EXPECT_LT(comparison.with_skips.stats.comparisons,
              comparison.linear.stats.comparisons / 10);
    EXPECT_GT(comparison.with_skips.stats.skip_jumps, 0U);
}

// Проверяется ускорение поиска разреженных docID в плотном posting list.
TEST(BooleanSearchComparisonTest, SparseDocuments) {
    const BooleanPostingList left =
        make_posting_list(make_sequence(DOCUMENT_COUNT), 16);
    const BooleanPostingList right =
        make_posting_list(make_sparse(DOCUMENT_COUNT, 5000), 16);

    const SearchComparison comparison = measure_search(left, right);
    print_comparison("2. Разреженные docID в плотном списке", comparison);

    EXPECT_EQ(comparison.with_skips.documents, comparison.linear.documents);
    EXPECT_EQ(comparison.linear.documents, right.documents);
    EXPECT_LT(comparison.with_skips.stats.comparisons,
              comparison.linear.stats.comparisons);
    EXPECT_GT(comparison.with_skips.stats.skip_jumps, 0U);
}

// Проверяется отказ от прыжков, когда docID двух posting lists идут почти подряд.
TEST(BooleanSearchComparisonTest, InterleavedDocuments) {
    BooleanResult even;
    BooleanResult odd;
    even.reserve(DOCUMENT_COUNT / 2);
    odd.reserve(DOCUMENT_COUNT / 2);
    for (uint32_t document = 0; document < DOCUMENT_COUNT; ++document) {
        (document % 2 == 0 ? even : odd).push_back(document);
    }
    const BooleanPostingList left =
        make_posting_list(std::move(even), 16);
    const BooleanPostingList right =
        make_posting_list(std::move(odd), 16);

    const SearchComparison comparison = measure_search(left, right);
    print_comparison("3. Чередующиеся docID без совпадений", comparison);

    EXPECT_EQ(comparison.with_skips.documents, comparison.linear.documents);
    EXPECT_TRUE(comparison.linear.documents.empty());
    EXPECT_EQ(comparison.with_skips.stats.comparisons,
              comparison.linear.stats.comparisons);
    EXPECT_EQ(comparison.with_skips.stats.skip_jumps, 0U);
}

}  // namespace
