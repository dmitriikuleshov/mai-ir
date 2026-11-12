#include <gtest/gtest.h>

#include "boolean_index/compression/variable_byte.hpp"
#include "boolean_search/index/index_reader.hpp"
#include "boolean_search/query/query_parser.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
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

class BooleanSearchTest : public testing::Test {
protected:
    void SetUp() override {
        std::snprintf(directory_, sizeof(directory_),
                      "/tmp/mai-ir-search-unit-XXXXXX");
        ASSERT_NE(mkdtemp(directory_), nullptr);
        ASSERT_NO_FATAL_FAILURE(create_index_fixture());

        char error[256] = "";
        ASSERT_TRUE(boolean_index_open(&index_, directory_, error, sizeof(error)))
            << error;
        dictionary_ = lemma_dictionary_load(
            MAI_IR_SOURCE_DIR "/resources/wordnet_exceptions.tsv",
            MAI_IR_SOURCE_DIR "/resources/wordnet_lemmas.txt",
            MAI_IR_SOURCE_DIR "/resources/technical_lemma_overrides.tsv");
        ASSERT_NE(dictionary_, nullptr);
    }

    void TearDown() override {
        lemma_dictionary_destroy(dictionary_);
        boolean_index_close(&index_);
        const char *files[] = {"terms.bin", "postings.bin", "lexicon.bin",
                               "documents.bin"};
        for (const char *name : files) {
            char path[PATH_CAPACITY];
            path_join(path, sizeof(path), directory_, name);
            unlink(path);
        }
        rmdir(directory_);
    }

    void expect_query(const char *query, const uint32_t *expected,
                      size_t expected_size,
                      size_t maximum_token_length = 100) {
        char error[256] = "";
        BooleanResult result = boolean_search(
            &index_, query, maximum_token_length, dictionary_, error,
            sizeof(error));
        ASSERT_STREQ(error, "") << "query: " << query;
        ASSERT_EQ(result.size(), expected_size) << "query: " << query;
        for (size_t index = 0; index < expected_size; ++index) {
            EXPECT_EQ(result[index], expected[index])
                << "query: " << query << ", position: " << index;
        }
    }

    void expect_invalid_query(const char *query,
                              size_t maximum_token_length = 100) {
        char error[256] = "";
        BooleanResult result = boolean_search(
            &index_, query, maximum_token_length, dictionary_, error,
            sizeof(error));
        EXPECT_STRNE(error, "") << "query: " << query;
        EXPECT_TRUE(result.empty()) << "query: " << query;
    }

    BooleanIndex *index() { return &index_; }
    const char *directory() const { return directory_; }

private:
    void create_index_fixture() {
        const char *terms[] = {"c++",       "cloud",    "database",
                               "node.js",   "query",    "retrieval",
                               "security",  "sha-256",  "storage"};
        const uint32_t cpp[] = {0, 3};
        const uint32_t cloud[] = {0, 1, 5};
        const uint32_t database[] = {1, 2, 5};
        const uint32_t node[] = {2, 3};
        const uint32_t query[] = {2, 3};
        const uint32_t retrieval[] = {0, 2, 4};
        const uint32_t security[] = {1, 4, 5};
        const uint32_t sha[] = {3, 5};
        const uint32_t storage[] = {2, 4};
        const uint32_t *lists[] = {cpp,      cloud,    database,
                                   node,     query,    retrieval,
                                   security, sha,      storage};
        const uint32_t sizes[] = {2, 3, 3, 2, 2, 3, 3, 2, 2};

        char path[PATH_CAPACITY];
        path_join(path, sizeof(path), directory_, "terms.bin");
        FILE *terms_file = std::fopen(path, "wb");
        path_join(path, sizeof(path), directory_, "postings.bin");
        FILE *postings_file = std::fopen(path, "wb");
        path_join(path, sizeof(path), directory_, "lexicon.bin");
        FILE *lexicon_file = std::fopen(path, "wb");
        path_join(path, sizeof(path), directory_, "documents.bin");
        FILE *documents_file = std::fopen(path, "wb");
        ASSERT_NE(terms_file, nullptr);
        ASSERT_NE(postings_file, nullptr);
        ASSERT_NE(lexicon_file, nullptr);
        ASSERT_NE(documents_file, nullptr);

        boolean_index_format::IndexHeader header{};
        std::memcpy(header.magic, boolean_index_format::INDEX_MAGIC,
                    sizeof(header.magic));
        header.version = boolean_index_format::VERSION;
        header.document_count = 6;
        header.term_count = 9;
        header.posting_count = 22;
        ASSERT_TRUE(write_exact(lexicon_file, &header, sizeof(header)));

        uint64_t term_offset = 0;
        uint64_t postings_offset = 0;
        for (size_t index = 0; index < 9; ++index) {
            const uint32_t term_length =
                static_cast<uint32_t>(std::strlen(terms[index]));
            const boolean_index_format::LexiconEntry entry{
                term_offset, postings_offset, term_length, sizes[index]};
            ASSERT_TRUE(write_exact(lexicon_file, &entry, sizeof(entry)));
            ASSERT_TRUE(write_exact(terms_file, terms[index], term_length));
            uint32_t previous_document = 0;
            for (uint32_t position = 0; position < sizes[index]; ++position) {
                const uint32_t document_gap =
                    position == 0
                        ? lists[index][position]
                        : lists[index][position] - previous_document;
                ASSERT_TRUE(variable_byte_write(postings_file, document_gap));
                ASSERT_TRUE(variable_byte_write(postings_file, position + 1));
                previous_document = lists[index][position];
            }
            term_offset += term_length;
            postings_offset = static_cast<uint64_t>(ftello(postings_file));
        }

        boolean_index_format::DocumentHeader document_header{};
        std::memcpy(document_header.magic,
                    boolean_index_format::DOCUMENT_MAGIC,
                    sizeof(document_header.magic));
        document_header.version = boolean_index_format::VERSION;
        document_header.document_count = 6;
        ASSERT_TRUE(write_exact(documents_file, &document_header,
                                sizeof(document_header)));
        uint8_t object_ids[6][12]{};
        for (size_t index = 0; index < 6; ++index) {
            object_ids[index][11] = static_cast<uint8_t>(index + 1);
        }
        ASSERT_TRUE(write_exact(documents_file, object_ids, sizeof(object_ids)));

        ASSERT_EQ(std::fclose(terms_file), 0);
        ASSERT_EQ(std::fclose(postings_file), 0);
        ASSERT_EQ(std::fclose(lexicon_file), 0);
        ASSERT_EQ(std::fclose(documents_file), 0);
    }

    char directory_[128]{};
    BooleanIndex index_{};
    LemmaDictionary *dictionary_ = nullptr;
};

// Проверяется открытие индекса и перевод граничных docID в ObjectId.
TEST_F(BooleanSearchTest, OpensIndexAndReadsDocumentObjectIds) {
    EXPECT_EQ(index()->header.document_count, 6U);
    EXPECT_EQ(index()->header.term_count, 9U);
    EXPECT_EQ(index()->header.posting_count, 22U);

    uint8_t object_id[12]{};
    ASSERT_TRUE(boolean_index_document_oid(index(), 0, object_id));
    EXPECT_EQ(object_id[11], 1U);
    ASSERT_TRUE(boolean_index_document_oid(index(), 5, object_id));
    EXPECT_EQ(object_id[11], 6U);
    EXPECT_FALSE(boolean_index_document_oid(index(), 6, object_id));
}

// Проверяется чтение из сжатого posting list как docID, так и будущего TF.
TEST_F(BooleanSearchTest, ReadsDocumentIdsAndTermFrequencies) {
    const WeightedPostingList postings =
        load_weighted_postings(index(), "cloud");

    ASSERT_EQ(postings.size(), 3U);
    EXPECT_EQ(postings[0].document_id, 0U);
    EXPECT_EQ(postings[0].term_frequency, 1U);
    EXPECT_EQ(postings[1].document_id, 1U);
    EXPECT_EQ(postings[1].term_frequency, 2U);
    EXPECT_EQ(postings[2].document_id, 5U);
    EXPECT_EQ(postings[2].term_frequency, 3U);
}

// Проверяется поиск обычного, отсутствующего и лемматизированного терма.
TEST_F(BooleanSearchTest, FindsSingleMissingAndLemmatizedTerms) {
    const uint32_t cloud[] = {0, 1, 5};
    const uint32_t queries[] = {2, 3};
    expect_query("cloud", cloud, 3);
    expect_query("unknown", nullptr, 0);
    expect_query("queries", queries, 2);
    expect_query("QUERIES", queries, 2);
}

// Проверяется поиск технических термов с регистром и Unicode-дефисом.
TEST_F(BooleanSearchTest, FindsTechnicalTermsAndNormalizesTheirNotation) {
    const uint32_t cpp_and_node[] = {3};
    const uint32_t node_or_sha[] = {2, 3, 5};
    const uint32_t sha[] = {3, 5};

    expect_query("C++ AND Node.js", cpp_and_node, 1);
    expect_query("node.js OR sha-256", node_or_sha, 3);
    expect_query("SHA‑256", sha, 2);
}

// Проверяется корректность базовых операций AND, OR и NOT.
TEST_F(BooleanSearchTest, ExecutesAndOrAndNotOperations) {
    const uint32_t intersection[] = {1, 5};
    const uint32_t union_result[] = {0, 1, 4, 5};
    const uint32_t complement[] = {2, 3, 4};
    const uint32_t combined[] = {2, 4};
    expect_query("cloud AND database", intersection, 2);
    expect_query("cloud OR security", union_result, 4);
    expect_query("NOT cloud", complement, 3);
    expect_query("NOT cloud AND retrieval", combined, 2);
}

// Проверяется идемпотентность повторного терма в AND и OR.
TEST_F(BooleanSearchTest, TreatsRepeatedOperandsAsIdempotent) {
    const uint32_t cloud[] = {0, 1, 5};

    expect_query("cloud AND cloud", cloud, 3);
    expect_query("cloud OR cloud", cloud, 3);
    expect_query("(cloud AND cloud) OR cloud", cloud, 3);
}

// Проверяется поведение неизвестного терма во всех булевых операциях.
TEST_F(BooleanSearchTest, HandlesUnknownTermsInBooleanOperations) {
    const uint32_t cloud[] = {0, 1, 5};
    const uint32_t all_documents[] = {0, 1, 2, 3, 4, 5};

    expect_query("unknown AND cloud", nullptr, 0);
    expect_query("unknown OR cloud", cloud, 3);
    expect_query("NOT unknown", all_documents, 6);
    expect_query("NOT cloud AND unknown", nullptr, 0);
}

// Проверяется применение чётного и нечётного числа операторов NOT.
TEST_F(BooleanSearchTest, AppliesRepeatedNotOperators) {
    const uint32_t cloud[] = {0, 1, 5};
    const uint32_t not_cloud[] = {2, 3, 4};

    expect_query("NOT NOT cloud", cloud, 3);
    expect_query("NOT NOT NOT cloud", not_cloud, 3);
}

// Проверяется приоритет NOT, AND, OR и влияние круглых скобок.
TEST_F(BooleanSearchTest, AppliesNotAndOrPrecedenceAndParentheses) {
    const uint32_t without_parentheses[] = {0, 1, 5};
    const uint32_t with_parentheses[] = {1, 5};
    const uint32_t nested[] = {2, 3};
    expect_query("cloud OR database AND security", without_parentheses, 3);
    expect_query("(cloud OR database) AND security", with_parentheses, 2);
    expect_query("NOT (cloud OR security)", nested, 2);
}

// Проверяется разбор пробелов, табуляции, переноса строк и скобок без отступов.
TEST_F(BooleanSearchTest, HandlesWhitespaceAndAdjacentParentheses) {
    const uint32_t expected[] = {1, 5};

    expect_query("\t(cloud\nOR\tdatabase)\nAND security ", expected, 2);
}

// Проверяется вычисление вложенного выражения с положительными и отрицательными термами.
TEST_F(BooleanSearchTest, EvaluatesNestedExpressionWithPositiveAndNegativeTerms) {
    const uint32_t expected[] = {0, 2};

    expect_query("(cloud AND NOT database) OR "
                 "(storage AND NOT security)",
                 expected, 2);
}

// Проверяется распознавание булевых операторов независимо от регистра.
TEST_F(BooleanSearchTest, RecognizesOperatorsCaseInsensitively) {
    const uint32_t expected[] = {1, 5};
    expect_query("cloud aNd database", expected, 2);
}

// Проверяется отказ на разных синтаксически некорректных запросах.
TEST_F(BooleanSearchTest, ReportsSyntaxErrorsWithoutReturningDocuments) {
    const char *invalid[] = {"",
                             "   ",
                             "()",
                             "( )",
                             "NOT",
                             "cloud AND",
                             "AND cloud",
                             "(cloud OR database",
                             "cloud database",
                             "cloud OR OR database",
                             "cloud AND ()",
                             "cloud OR (database AND)",
                             "cloud NOT database",
                             "cloud)"};
    for (const char *query : invalid) {
        SCOPED_TRACE(query);
        expect_invalid_query(query);
    }
}

// Проверяется отказ, если поисковый терм даёт ноль или несколько токенов.
TEST_F(BooleanSearchTest, RejectsTermsThatDoNotNormalizeToExactlyOneToken) {
    expect_invalid_query("cloud/database");
    expect_invalid_query("...");
    expect_invalid_query("database", 4);
}

// Проверяется отказ от открытия индекса с повреждённой сигнатурой.
TEST_F(BooleanSearchTest, RejectsCorruptIndexHeader) {
    boolean_index_close(index());
    char path[PATH_CAPACITY];
    path_join(path, sizeof(path), directory(), "lexicon.bin");
    FILE *file = std::fopen(path, "rb+");
    ASSERT_NE(file, nullptr);
    boolean_index_format::IndexHeader header{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, file), 1U);
    header.magic[0] = 'X';
    ASSERT_EQ(fseeko(file, 0, SEEK_SET), 0);
    ASSERT_TRUE(write_exact(file, &header, sizeof(header)));
    ASSERT_EQ(std::fclose(file), 0);

    char error[256] = "";
    EXPECT_FALSE(boolean_index_open(index(), directory(), error, sizeof(error)));
    EXPECT_STRNE(error, "");
}

// Проверяется отказ при различном числе документов в индексе и таблице ObjectId.
TEST_F(BooleanSearchTest, RejectsMismatchedDocumentTable) {
    boolean_index_close(index());
    char path[PATH_CAPACITY];
    path_join(path, sizeof(path), directory(), "documents.bin");
    FILE *file = std::fopen(path, "rb+");
    ASSERT_NE(file, nullptr);
    boolean_index_format::DocumentHeader header{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, file), 1U);
    header.document_count = 5;
    ASSERT_EQ(fseeko(file, 0, SEEK_SET), 0);
    ASSERT_TRUE(write_exact(file, &header, sizeof(header)));
    ASSERT_EQ(std::fclose(file), 0);

    char error[256] = "";
    EXPECT_FALSE(boolean_index_open(index(), directory(), error, sizeof(error)));
    EXPECT_STRNE(error, "");
}

// Проверяется понятная ошибка при отсутствии каталога с файлами индекса.
TEST(BooleanIndexReaderTest, RejectsMissingIndexDirectory) {
    BooleanIndex index{};
    char error[256] = "";
    EXPECT_FALSE(boolean_index_open(&index, "/tmp/mai-ir-index-does-not-exist",
                                    error, sizeof(error)));
    EXPECT_STRNE(error, "");
    boolean_index_close(&index);
}

}  // namespace
