#include <gtest/gtest.h>

#include "boolean_search/query/posting_operations.hpp"

namespace {

BooleanResult sequence(uint32_t begin, uint32_t end) {
    BooleanResult result;
    result.reserve(end - begin);
    for (uint32_t value = begin; value < end; ++value) {
        result.push_back(value);
    }
    return result;
}

// Проверяется эвристика преподавателя: для 100 postings шаг равен sqrt(100)=10.
TEST(SkipPointersTest, BuildsEvenSquareRootSkips) {
    const BooleanPostingList list = make_posting_list(sequence(0, 100), 16);

    ASSERT_EQ(list.skips.size(), 9U);
    EXPECT_EQ(list.skips.front().posting_index, 10U);
    EXPECT_EQ(list.skips.front().document_id, 10U);
    EXPECT_EQ(list.skips.back().posting_index, 90U);
    EXPECT_EQ(list.skips.back().document_id, 90U);
}

// Проверяется отсутствие бесполезных указателей у короткого posting list.
TEST(SkipPointersTest, LeavesShortPostingListWithoutSkips) {
    const BooleanPostingList list = make_posting_list(sequence(0, 15), 16);

    EXPECT_TRUE(list.skips.empty());
}

// Проверяется полное совпадение результатов линейного AND и AND с прыжками.
TEST(SkipPointersTest, ProducesSameIntersectionAsLinearAlgorithm) {
    BooleanResult even;
    BooleanResult multiples_of_three;
    for (uint32_t value = 0; value < 1000; ++value) {
        if (value % 2 == 0) even.push_back(value);
        if (value % 3 == 0) multiples_of_three.push_back(value);
    }
    const BooleanPostingList left = make_posting_list(std::move(even), 16);
    const BooleanPostingList right =
        make_posting_list(std::move(multiples_of_three), 16);

    const BooleanResult accelerated =
        intersect_postings(left, right, true, nullptr);
    const BooleanResult linear =
        intersect_postings(left, right, false, nullptr);

    EXPECT_EQ(accelerated, linear);
    ASSERT_FALSE(accelerated.empty());
    EXPECT_EQ(accelerated.front(), 0U);
    EXPECT_EQ(accelerated.back(), 996U);
}

// Проверяется переход точно к совпадающему docID без потери результата.
TEST(SkipPointersTest, LandsOnMatchingDocument) {
    const BooleanPostingList long_list =
        make_posting_list(sequence(0, 1000), 16);
    const BooleanPostingList target{{900}, {}};
    IntersectionStats stats{};

    const BooleanResult result =
        intersect_postings(long_list, target, true, &stats);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0], 900U);
    EXPECT_GT(stats.skip_jumps, 0U);
}

// Проверяется измеримое сокращение сравнений на сильно неравных списках.
TEST(SkipPointersTest, ReducesComparisonsForDistantDocument) {
    const BooleanPostingList long_list =
        make_posting_list(sequence(0, 10000), 16);
    const BooleanPostingList target{{9999}, {}};
    IntersectionStats linear_stats{};
    IntersectionStats skip_stats{};

    const BooleanResult linear =
        intersect_postings(long_list, target, false, &linear_stats);
    const BooleanResult accelerated =
        intersect_postings(long_list, target, true, &skip_stats);

    EXPECT_EQ(accelerated, linear);
    EXPECT_LT(skip_stats.comparisons, linear_stats.comparisons / 10);
    EXPECT_GT(skip_stats.skip_jumps, 0U);
    EXPECT_GT(skip_stats.skipped_postings, 9000U);
}

// Проверяется корректность, когда прыгать нельзя из-за близких docID.
TEST(SkipPointersTest, FallsBackToSequentialAdvanceForCloseDocuments) {
    const BooleanPostingList left = make_posting_list(sequence(0, 100), 16);
    const BooleanPostingList right = make_posting_list(sequence(1, 101), 16);

    const BooleanResult result = intersect_postings(left, right, true, nullptr);

    ASSERT_EQ(result.size(), 99U);
    EXPECT_EQ(result.front(), 1U);
    EXPECT_EQ(result.back(), 99U);
}

// Проверяется безопасная работа с пустым posting list с обеих сторон.
TEST(SkipPointersTest, HandlesEmptyPostingLists) {
    const BooleanPostingList empty{};
    const BooleanPostingList values = make_posting_list(sequence(0, 100), 16);

    EXPECT_TRUE(intersect_postings(empty, values, true, nullptr).empty());
    EXPECT_TRUE(intersect_postings(values, empty, true, nullptr).empty());
}

}  // namespace
