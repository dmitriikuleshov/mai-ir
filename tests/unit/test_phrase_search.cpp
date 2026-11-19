#include <gtest/gtest.h>

#include "boolean_index/storage/files.hpp"
#include "boolean_index/storage/format.hpp"
#include "phrase_search/index/index_reader.hpp"
#include "phrase_search/query/search.hpp"
#include "positional_index/building/blocks.hpp"
#include "positional_index/storage/writer.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

namespace {

PositionPostingList postings(
    std::initializer_list<PositionPosting> values) {
    return PositionPostingList(values);
}

void remove_directory_files(const char *directory) {
    DIR *stream = opendir(directory);
    if (stream == nullptr) return;
    while (dirent *entry = readdir(stream)) {
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) continue;
        char path[PATH_SIZE];
        join_path(path, sizeof(path), directory, entry->d_name);
        unlink(path);
    }
    closedir(stream);
}

class PhraseSearchBinaryIndexTest : public testing::Test {
protected:
    void SetUp() override {
        std::snprintf(directory_, sizeof(directory_),
                      "/tmp/mai-ir-phrase-search-XXXXXX");
        ASSERT_NE(mkdtemp(directory_), nullptr);
        ASSERT_NO_FATAL_FAILURE(create_index());
        char error[256] = "";
        ASSERT_TRUE(positional_index_open(&index_, directory_, directory_,
                                          error, sizeof(error)))
            << error;
        dictionary_ = lemma_dictionary_load(
            MAI_IR_SOURCE_DIR "/resources/wordnet_exceptions.tsv",
            MAI_IR_SOURCE_DIR "/resources/wordnet_lemmas.txt",
            MAI_IR_SOURCE_DIR "/resources/technical_lemma_overrides.tsv");
        ASSERT_NE(dictionary_, nullptr);
    }

    void TearDown() override {
        lemma_dictionary_destroy(dictionary_);
        positional_index_close(&index_);
        remove_directory_files(directory_);
        rmdir(directory_);
    }

    PhraseSearchResult search(const char *query) {
        char error[256] = "";
        PhraseSearchResult result = phrase_search(
            &index_, query, 100, dictionary_, error, sizeof(error));
        EXPECT_STREQ(error, "");
        return result;
    }

private:
    void create_index() {
        PositionBuffer buffer;
        const uint32_t cloud[][2] = {
            {0, 0}, {0, 2}, {1, 1}, {2, 0}, {3, 0}, {3, 1}, {3, 2}};
        for (const auto &value : cloud) {
            position_buffer_add(&buffer, "cloud", value[0], value[1]);
        }
        position_buffer_add(&buffer, "database", 0, 1);
        position_buffer_add(&buffer, "database", 1, 0);
        position_buffer_add(&buffer, "security", 0, 3);
        position_buffer_add(&buffer, "security", 1, 2);
        position_buffer_add(&buffer, "security", 2, 2);
        PositionBlockList blocks;
        flush_position_block(&buffer, &blocks, directory_);
        merge_position_blocks(&blocks, directory_, 4);
        for (const std::string &path : blocks) unlink(path.c_str());

        char path[PATH_SIZE];
        join_path(path, sizeof(path), directory_, "documents.bin");
        FILE *file = std::fopen(path, "wb");
        ASSERT_NE(file, nullptr);
        boolean_index_format::DocumentHeader header{};
        std::memcpy(header.magic, boolean_index_format::DOCUMENT_MAGIC, 8);
        header.version = boolean_index_format::VERSION;
        header.document_count = 4;
        ASSERT_EQ(std::fwrite(&header, sizeof(header), 1, file), 1U);
        uint8_t ids[4][12]{};
        ids[0][11] = 1;
        ids[1][11] = 2;
        ids[2][11] = 3;
        ids[3][11] = 4;
        ASSERT_EQ(std::fwrite(ids, sizeof(ids), 1, file), 1U);
        ASSERT_EQ(std::fclose(file), 0);
    }

    char directory_[128]{};
    PositionalIndex index_{};
    LemmaDictionary *dictionary_ = nullptr;
};

}  // namespace

// Проверяется точное совпадение двух соседних термов в нескольких документах.
TEST(PhraseIntersectionTest, FindsOnlyAdjacentTerms) {
    const std::vector<PositionPostingList> terms = {
        postings({{0, {0, 2}}, {1, {1}}, {2, {0}}}),
        postings({{0, {3}}, {1, {2}}, {2, {2}}}),
    };

    const PhraseSearchResult result = intersect_phrase_positions(terms);

    ASSERT_EQ(result.documents.size(), 2U);
    EXPECT_EQ(result.occurrence_count, 2U);
    EXPECT_EQ(result.documents[0].document_id, 0U);
    EXPECT_EQ(result.documents[0].start_positions,
              (std::vector<uint32_t>{2}));
    EXPECT_EQ(result.documents[1].document_id, 1U);
    EXPECT_EQ(result.documents[1].start_positions,
              (std::vector<uint32_t>{1}));
}

// Проверяется, что правильные слова с промежутком не считаются точной цитатой.
TEST(PhraseIntersectionTest, RejectsTermsSeparatedByAnotherWord) {
    const std::vector<PositionPostingList> terms = {
        postings({{7, {10}}}), postings({{7, {12}}})};

    const PhraseSearchResult result = intersect_phrase_positions(terms);

    EXPECT_TRUE(result.documents.empty());
    EXPECT_EQ(result.occurrence_count, 0U);
}

// Проверяется, что перестановка слов изменяет результат цитатного поиска.
TEST(PhraseIntersectionTest, PreservesTermOrder) {
    const PositionPostingList cloud = postings({{3, {4}}});
    const PositionPostingList security = postings({{3, {5}}});

    EXPECT_EQ(intersect_phrase_positions({cloud, security}).documents.size(),
              1U);
    EXPECT_TRUE(
        intersect_phrase_positions({security, cloud}).documents.empty());
}

// Проверяется последовательное пересечение всех трёх термов цитаты.
TEST(PhraseIntersectionTest, MatchesThreeWordPhrase) {
    const std::vector<PositionPostingList> terms = {
        postings({{0, {0}}, {1, {4}}}),
        postings({{0, {1}}, {1, {5}}}),
        postings({{0, {2}}, {1, {7}}}),
    };

    const PhraseSearchResult result = intersect_phrase_positions(terms);

    ASSERT_EQ(result.documents.size(), 1U);
    EXPECT_EQ(result.documents[0].document_id, 0U);
    EXPECT_EQ(result.documents[0].start_positions,
              (std::vector<uint32_t>{0}));
}

// Проверяется несколько вхождений одной цитаты внутри одного документа.
TEST(PhraseIntersectionTest, KeepsEveryOccurrenceInDocument) {
    const std::vector<PositionPostingList> terms = {
        postings({{2, {0, 4, 8}}}), postings({{2, {1, 5, 9}}})};

    const PhraseSearchResult result = intersect_phrase_positions(terms);

    ASSERT_EQ(result.documents.size(), 1U);
    EXPECT_EQ(result.occurrence_count, 3U);
    EXPECT_EQ(result.documents[0].start_positions,
              (std::vector<uint32_t>{0, 4, 8}));
}

// Проверяется цитата из повторяющегося терма: cloud cloud cloud.
TEST(PhraseIntersectionTest, HandlesRepeatedTermsAndOverlappingMatches) {
    const PositionPostingList cloud = postings({{5, {0, 1, 2, 3}}});

    const PhraseSearchResult result =
        intersect_phrase_positions({cloud, cloud, cloud});

    ASSERT_EQ(result.documents.size(), 1U);
    EXPECT_EQ(result.occurrence_count, 2U);
    EXPECT_EQ(result.documents[0].start_positions,
              (std::vector<uint32_t>{0, 1}));
}

// Проверяется односоставная цитата: каждое вхождение терма остаётся результатом.
TEST(PhraseIntersectionTest, SupportsSingleTermPhrase) {
    const PhraseSearchResult result = intersect_phrase_positions(
        {postings({{1, {2, 7}}, {4, {0}}})});

    ASSERT_EQ(result.documents.size(), 2U);
    EXPECT_EQ(result.occurrence_count, 3U);
    EXPECT_EQ(result.documents[0].start_positions,
              (std::vector<uint32_t>{2, 7}));
    EXPECT_EQ(result.documents[1].start_positions,
              (std::vector<uint32_t>{0}));
}

// Проверяется пустой результат, когда хотя бы один терм отсутствует в корпусе.
TEST(PhraseIntersectionTest, ReturnsEmptyResultForMissingTerm) {
    const PhraseSearchResult result = intersect_phrase_positions(
        {postings({{0, {1}}}), PositionPostingList{}});

    EXPECT_TRUE(result.documents.empty());
    EXPECT_EQ(result.occurrence_count, 0U);
}

// Проверяется полный путь: кавычки, лемматизация, чтение координат и пересечение.
TEST_F(PhraseSearchBinaryIndexTest, SearchesQuotedPhraseInBinaryIndex) {
    const PhraseSearchResult result = search("\"clouds security\"");

    ASSERT_EQ(result.documents.size(), 2U);
    EXPECT_EQ(result.occurrence_count, 2U);
    EXPECT_EQ(result.documents[0].document_id, 0U);
    EXPECT_EQ(result.documents[0].start_positions,
              (std::vector<uint32_t>{2}));
    EXPECT_EQ(result.documents[1].document_id, 1U);
    EXPECT_EQ(result.documents[1].start_positions,
              (std::vector<uint32_t>{1}));
}

// Проверяется полный поиск перекрывающихся повторов одной и той же леммы.
TEST_F(PhraseSearchBinaryIndexTest, FindsRepeatedTermPhraseFromDisk) {
    const PhraseSearchResult result = search("cloud cloud");

    ASSERT_EQ(result.documents.size(), 1U);
    EXPECT_EQ(result.documents[0].document_id, 3U);
    EXPECT_EQ(result.documents[0].start_positions,
              (std::vector<uint32_t>{0, 1}));
    EXPECT_EQ(result.occurrence_count, 2U);
}

// Проверяется корректный пустой ответ для неизвестного терма без ошибки индекса.
TEST_F(PhraseSearchBinaryIndexTest, ReturnsEmptyForUnknownPhraseTerm) {
    const PhraseSearchResult result = search("cloud nonexistentterm");

    EXPECT_TRUE(result.documents.empty());
    EXPECT_EQ(result.occurrence_count, 0U);
}
