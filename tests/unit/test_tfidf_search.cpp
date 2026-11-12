#include <gtest/gtest.h>

#include "boolean_index/compression/variable_byte.hpp"
#include "boolean_index/storage/format.hpp"
#include "tfidf_search/index/ranking_index.hpp"
#include "tfidf_search/ranking/search.hpp"
#include "tfidf_search/ranking/weights.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

constexpr size_t PATH_CAPACITY = 4096;

void path_join(char *target, size_t capacity, const char *directory,
               const char *name) {
    std::snprintf(target, capacity, "%s/%s", directory, name);
}

bool write_exact(FILE *file, const void *data, size_t size) {
    return std::fwrite(data, 1, size, file) == size;
}

class TfidfSearchTest : public testing::Test {
protected:
    void SetUp() override {
        std::snprintf(directory_, sizeof(directory_),
                      "/tmp/mai-ir-tfidf-unit-XXXXXX");
        ASSERT_NE(mkdtemp(directory_), nullptr);
        ASSERT_NO_FATAL_FAILURE(create_index_fixture());

        char error[256] = "";
        ASSERT_TRUE(tfidf_index_open(&index_, directory_, error, sizeof(error)))
            << error;
        dictionary_ = lemma_dictionary_load(
            MAI_IR_SOURCE_DIR "/resources/wordnet_exceptions.tsv",
            MAI_IR_SOURCE_DIR "/resources/wordnet_lemmas.txt",
            MAI_IR_SOURCE_DIR "/resources/technical_lemma_overrides.tsv");
        ASSERT_NE(dictionary_, nullptr);
    }

    void TearDown() override {
        lemma_dictionary_destroy(dictionary_);
        tfidf_index_close(&index_);
        const char *files[] = {"terms.bin", "postings.bin", "lexicon.bin",
                               "documents.bin", "norms.bin"};
        for (const char *name : files) {
            char path[PATH_CAPACITY];
            path_join(path, sizeof(path), directory_, name);
            unlink(path);
        }
        rmdir(directory_);
    }

    TfidfSearchResult search(const char *query, size_t limit = 20) {
        char error[256] = "";
        TfidfSearchResult result = tfidf_search(
            &index_, query, 100, limit, dictionary_, error, sizeof(error));
        EXPECT_STREQ(error, "");
        return result;
    }

    TfidfIndex *index() { return &index_; }
    const LemmaDictionary *dictionary() const { return dictionary_; }
    const char *directory() const { return directory_; }

private:
    void create_index_fixture() {
        const char *terms[] = {"cloud", "database", "security"};
        const uint32_t cloud_documents[] = {0, 1};
        const uint32_t database_documents[] = {0, 2};
        const uint32_t security_documents[] = {1, 2};
        const uint32_t cloud_frequencies[] = {2, 1};
        const uint32_t database_frequencies[] = {1, 2};
        const uint32_t security_frequencies[] = {3, 1};
        const uint32_t *documents[] = {
            cloud_documents, database_documents, security_documents};
        const uint32_t *frequencies[] = {
            cloud_frequencies, database_frequencies, security_frequencies};

        char path[PATH_CAPACITY];
        path_join(path, sizeof(path), directory_, "terms.bin");
        FILE *terms_file = std::fopen(path, "wb");
        path_join(path, sizeof(path), directory_, "postings.bin");
        FILE *postings_file = std::fopen(path, "wb");
        path_join(path, sizeof(path), directory_, "lexicon.bin");
        FILE *lexicon_file = std::fopen(path, "wb");
        path_join(path, sizeof(path), directory_, "documents.bin");
        FILE *documents_file = std::fopen(path, "wb");
        path_join(path, sizeof(path), directory_, "norms.bin");
        FILE *norms_file = std::fopen(path, "wb");
        ASSERT_NE(terms_file, nullptr);
        ASSERT_NE(postings_file, nullptr);
        ASSERT_NE(lexicon_file, nullptr);
        ASSERT_NE(documents_file, nullptr);
        ASSERT_NE(norms_file, nullptr);

        boolean_index_format::IndexHeader index_header{};
        std::memcpy(index_header.magic, boolean_index_format::INDEX_MAGIC, 8);
        index_header.version = boolean_index_format::VERSION;
        index_header.document_count = 3;
        index_header.term_count = 3;
        index_header.posting_count = 6;
        ASSERT_TRUE(write_exact(lexicon_file, &index_header,
                                sizeof(index_header)));

        uint64_t term_offset = 0;
        uint64_t postings_offset = 0;
        for (size_t term_index = 0; term_index < 3; ++term_index) {
            const uint32_t term_length =
                static_cast<uint32_t>(std::strlen(terms[term_index]));
            const boolean_index_format::LexiconEntry entry{
                term_offset, postings_offset, term_length, 2};
            ASSERT_TRUE(write_exact(lexicon_file, &entry, sizeof(entry)));
            ASSERT_TRUE(write_exact(terms_file, terms[term_index], term_length));

            uint32_t previous_document = 0;
            for (size_t position = 0; position < 2; ++position) {
                const uint32_t document_id = documents[term_index][position];
                const uint32_t gap = position == 0
                                         ? document_id
                                         : document_id - previous_document;
                ASSERT_TRUE(variable_byte_write(postings_file, gap));
                ASSERT_TRUE(variable_byte_write(
                    postings_file, frequencies[term_index][position]));
                previous_document = document_id;
            }
            term_offset += term_length;
            postings_offset = static_cast<uint64_t>(ftello(postings_file));
        }

        boolean_index_format::DocumentHeader document_header{};
        std::memcpy(document_header.magic,
                    boolean_index_format::DOCUMENT_MAGIC, 8);
        document_header.version = boolean_index_format::VERSION;
        document_header.document_count = 3;
        ASSERT_TRUE(write_exact(documents_file, &document_header,
                                sizeof(document_header)));
        uint8_t object_ids[3][12]{};
        object_ids[0][11] = 1;
        object_ids[1][11] = 2;
        object_ids[2][11] = 3;
        ASSERT_TRUE(write_exact(documents_file, object_ids, sizeof(object_ids)));

        boolean_index_format::DocumentNormHeader norm_header{};
        std::memcpy(norm_header.magic,
                    boolean_index_format::DOCUMENT_NORM_MAGIC, 8);
        norm_header.version = boolean_index_format::VERSION;
        norm_header.document_count = 3;
        ASSERT_TRUE(write_exact(norms_file, &norm_header, sizeof(norm_header)));
        const double tf2 = logarithmic_term_frequency(2);
        const double tf3 = logarithmic_term_frequency(3);
        const double norms[] = {std::sqrt(tf2 * tf2 + 1.0),
                                std::sqrt(1.0 + tf3 * tf3),
                                std::sqrt(tf2 * tf2 + 1.0)};
        ASSERT_TRUE(write_exact(norms_file, norms, sizeof(norms)));

        ASSERT_EQ(std::fclose(terms_file), 0);
        ASSERT_EQ(std::fclose(postings_file), 0);
        ASSERT_EQ(std::fclose(lexicon_file), 0);
        ASSERT_EQ(std::fclose(documents_file), 0);
        ASSERT_EQ(std::fclose(norms_file), 0);
    }

    char directory_[128]{};
    TfidfIndex index_{};
    LemmaDictionary *dictionary_ = nullptr;
};

// Проверяется логарифмическая частота терма: 0, 1, 10 и 100 вхождений.
TEST(TfidfWeightsTest, CalculatesLogarithmicTermFrequency) {
    EXPECT_DOUBLE_EQ(logarithmic_term_frequency(0), 0.0);
    EXPECT_DOUBLE_EQ(logarithmic_term_frequency(1), 1.0);
    EXPECT_NEAR(logarithmic_term_frequency(10), 2.0, 1e-12);
    EXPECT_NEAR(logarithmic_term_frequency(100), 3.0, 1e-12);
}

// Проверяется IDF для редкого, умеренно частого и общего терма корпуса.
TEST(TfidfWeightsTest, CalculatesInverseDocumentFrequency) {
    EXPECT_NEAR(inverse_document_frequency(100, 1), 2.0, 1e-12);
    EXPECT_NEAR(inverse_document_frequency(100, 10), 1.0, 1e-12);
    EXPECT_DOUBLE_EQ(inverse_document_frequency(100, 100), 0.0);
}

// Проверяется безопасный нулевой результат для некорректных значений N и df.
TEST(TfidfWeightsTest, RejectsInvalidDocumentFrequencies) {
    EXPECT_DOUBLE_EQ(inverse_document_frequency(0, 0), 0.0);
    EXPECT_DOUBLE_EQ(inverse_document_frequency(100, 0), 0.0);
    EXPECT_DOUBLE_EQ(inverse_document_frequency(100, 101), 0.0);
}

// Проверяется загрузка L2-норм в том же порядке, что и внутренние docID.
TEST_F(TfidfSearchTest, OpensIndexAndLoadsDocumentNorms) {
    ASSERT_EQ(index()->document_norms.size(), 3U);
    EXPECT_GT(index()->document_norms[0], 0.0);
    EXPECT_GT(index()->document_norms[1], 0.0);
    EXPECT_DOUBLE_EQ(index()->document_norms[0], index()->document_norms[2]);
}

// Проверяется ранжирование многословного запроса по формуле lnc.ltc.
TEST_F(TfidfSearchTest, RanksNaturalLanguageQueryByCosineScore) {
    const TfidfSearchResult result = search("cloud database");

    ASSERT_EQ(result.matched_documents, 3U);
    ASSERT_EQ(result.documents.size(), 3U);
    EXPECT_EQ(result.documents[0].document_id, 0U);
    EXPECT_EQ(result.documents[1].document_id, 2U);
    EXPECT_EQ(result.documents[2].document_id, 1U);

    const double tf2 = logarithmic_term_frequency(2);
    const double document_norm = std::sqrt(tf2 * tf2 + 1.0);
    const double query_norm = std::sqrt(2.0);
    EXPECT_NEAR(result.documents[0].score,
                (tf2 + 1.0) / (document_norm * query_norm), 1e-12);
    EXPECT_NEAR(result.documents[1].score,
                tf2 / (document_norm * query_norm), 1e-12);
}

// Проверяется ограничение выдачи без потери общего числа совпавших документов.
TEST_F(TfidfSearchTest, AppliesResultLimitAfterRanking) {
    const TfidfSearchResult result = search("cloud database", 2);

    EXPECT_EQ(result.matched_documents, 3U);
    ASSERT_EQ(result.documents.size(), 2U);
    EXPECT_EQ(result.documents[0].document_id, 0U);
    EXPECT_EQ(result.documents[1].document_id, 2U);
}

// Проверяется влияние частоты терма в документе на итоговый порядок.
TEST_F(TfidfSearchTest, UsesStoredDocumentTermFrequency) {
    const TfidfSearchResult result = search("security");

    ASSERT_EQ(result.documents.size(), 2U);
    EXPECT_EQ(result.documents[0].document_id, 1U);
    EXPECT_EQ(result.documents[1].document_id, 2U);
    EXPECT_GT(result.documents[0].score, result.documents[1].score);
}

// Проверяется, что повтор терма увеличивает его вес в векторе запроса.
TEST_F(TfidfSearchTest, UsesRepeatedQueryTermFrequency) {
    const TfidfSearchResult balanced = search("cloud database");
    const TfidfSearchResult cloud_focused = search("cloud cloud database");

    ASSERT_EQ(balanced.documents.size(), 3U);
    ASSERT_EQ(cloud_focused.documents.size(), 3U);
    EXPECT_GT(cloud_focused.documents[2].score, balanced.documents[2].score);
    EXPECT_LT(cloud_focused.documents[1].score, balanced.documents[1].score);
}

// Проверяется применение общей лемматизации к запросу ранжированного поиска.
TEST_F(TfidfSearchTest, LemmatizesQueryBeforeRanking) {
    const TfidfSearchResult result = search("databases");

    ASSERT_EQ(result.documents.size(), 2U);
    EXPECT_EQ(result.documents[0].document_id, 2U);
    EXPECT_EQ(result.documents[1].document_id, 0U);
}

// Проверяется корректный пустой результат для неизвестного терма.
TEST_F(TfidfSearchTest, ReturnsNoMatchesForUnknownTerm) {
    const TfidfSearchResult result = search("unknownterm");

    EXPECT_EQ(result.matched_documents, 0U);
    EXPECT_TRUE(result.documents.empty());
}

// Проверяется понятная ошибка для запроса без индексируемых термов.
TEST_F(TfidfSearchTest, RejectsQueryWithoutTerms) {
    char error[256] = "";
    const TfidfSearchResult result = tfidf_search(
        index(), "...", 100, 20, dictionary(), error, sizeof(error));

    EXPECT_STRNE(error, "");
    EXPECT_TRUE(result.documents.empty());
}

// Проверяется понятная ошибка вместо разыменования пустого указателя запроса.
TEST_F(TfidfSearchTest, RejectsNullQuery) {
    char error[256] = "";
    const TfidfSearchResult result = tfidf_search(
        index(), nullptr, 100, 20, dictionary(), error, sizeof(error));

    EXPECT_STRNE(error, "");
    EXPECT_TRUE(result.documents.empty());
}

// Проверяется отказ от запуска TF-IDF, если таблица норм отсутствует.
TEST_F(TfidfSearchTest, RejectsIndexWithoutDocumentNorms) {
    tfidf_index_close(index());
    char path[PATH_CAPACITY];
    path_join(path, sizeof(path), directory(), "norms.bin");
    ASSERT_EQ(unlink(path), 0);

    char error[256] = "";
    EXPECT_FALSE(tfidf_index_open(index(), directory(), error, sizeof(error)));
    EXPECT_STRNE(error, "");
}

}  // namespace
