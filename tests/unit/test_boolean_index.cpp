#include <gtest/gtest.h>

#include "boolean_index/building/blocks.hpp"
#include "boolean_index/building/builder.hpp"
#include "boolean_index/compression/variable_byte.hpp"
#include "boolean_index/storage/files.hpp"
#include "boolean_index/storage/format.hpp"
#include "boolean_index/storage/index_writer.hpp"
#include "tfidf_search/ranking/weights.hpp"

#include <bson/bson.h>
#include <cmath>
#include <cstring>
#include <dirent.h>

namespace {

void remove_directory_contents(const char *directory) {
    DIR *stream = opendir(directory);
    if (stream == nullptr) return;
    while (dirent *entry = readdir(stream)) {
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[PATH_SIZE];
        join_path(path, sizeof(path), directory, entry->d_name);
        unlink(path);
    }
    closedir(stream);
}

void destroy_blocks(BlockList *blocks) {
    for (const std::string &path : *blocks) {
        unlink(path.c_str());
    }
    blocks->clear();
}

bool read_raw_pair(FILE *file, char *term, size_t capacity,
                   uint32_t *document_id, uint32_t *term_frequency) {
    uint32_t length = 0;
    if (std::fread(&length, sizeof(length), 1, file) != 1) return false;
    if (static_cast<size_t>(length) + 1 > capacity ||
        std::fread(term, 1, length, file) != length ||
        std::fread(document_id, sizeof(*document_id), 1, file) != 1 ||
        std::fread(term_frequency, sizeof(*term_frequency), 1, file) != 1) {
        return false;
    }
    term[length] = '\0';
    return true;
}

bool read_term(FILE *terms, const boolean_index_format::LexiconEntry &entry,
               char *target, size_t capacity) {
    if (entry.term_length + 1 > capacity ||
        fseeko(terms, static_cast<off_t>(entry.term_offset), SEEK_SET) != 0 ||
        std::fread(target, 1, entry.term_length, terms) != entry.term_length) {
        return false;
    }
    target[entry.term_length] = '\0';
    return true;
}

class BooleanIndexTest : public testing::Test {
protected:
    void SetUp() override {
        std::snprintf(directory_, sizeof(directory_),
                      "/tmp/mai-ir-index-unit-XXXXXX");
        ASSERT_NE(mkdtemp(directory_), nullptr);
    }

    void TearDown() override {
        remove_directory_contents(directory_);
        rmdir(directory_);
    }

    const char *directory() const { return directory_; }

private:
    char directory_[128]{};
};

// Проверяется размер структур, версия и сигнатуры бинарного формата индекса.
TEST(BooleanIndexFormatTest, HasStablePackedBinaryLayout) {
    EXPECT_EQ(sizeof(boolean_index_format::IndexHeader), 32U);
    EXPECT_EQ(sizeof(boolean_index_format::LexiconEntry), 24U);
    EXPECT_EQ(sizeof(boolean_index_format::DocumentHeader), 16U);
    EXPECT_EQ(sizeof(boolean_index_format::DocumentNormHeader), 16U);
    EXPECT_EQ(boolean_index_format::VERSION, 3U);
    EXPECT_EQ(std::memcmp(boolean_index_format::INDEX_MAGIC, "MAIIRIDX", 8), 0);
    EXPECT_EQ(std::memcmp(boolean_index_format::DOCUMENT_MAGIC, "MAIIRDOC", 8),
              0);
}

// Проверяется извлечение лемм и идентификатора документа из BSON.
TEST_F(BooleanIndexTest, ExtractsTermsFromDocumentTermsBson) {
    bson_error_t error{};
    const char *json =
        "{\"lemmas\":[[\"database\",3],[\"cloud\",2],[\"security\",1]]}";
    bson_t *document = bson_new_from_json(
        reinterpret_cast<const uint8_t *>(json), -1, &error);
    ASSERT_NE(document, nullptr) << error.message;

    PostingBuffer buffer{};
    double document_norm = 0.0;
    ASSERT_TRUE(add_document_terms(document, "lemmas", 7, &buffer,
                                   &document_norm));
    ASSERT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer[0].term, "database");
    EXPECT_EQ(buffer[1].term, "cloud");
    EXPECT_EQ(buffer[2].term, "security");
    EXPECT_EQ(buffer[0].document_id, 7U);
    EXPECT_EQ(buffer[0].term_frequency, 3U);
    EXPECT_EQ(buffer[1].term_frequency, 2U);
    EXPECT_EQ(buffer[2].term_frequency, 1U);
    const double expected_norm = std::sqrt(
        std::pow(logarithmic_term_frequency(3), 2) +
        std::pow(logarithmic_term_frequency(2), 2) + 1.0);
    EXPECT_NEAR(document_norm, expected_norm, 1e-12);

    bson_destroy(document);
}

// Проверяется корректная обработка документа с пустым массивом термов.
TEST_F(BooleanIndexTest, AcceptsDocumentWithEmptyTermArray) {
    bson_error_t error{};
    bson_t *document = bson_new_from_json(
        reinterpret_cast<const uint8_t *>("{\"lemmas\":[]}"), -1, &error);
    ASSERT_NE(document, nullptr) << error.message;

    PostingBuffer buffer{};
    EXPECT_TRUE(add_document_terms(document, "lemmas", 4, &buffer));
    EXPECT_TRUE(buffer.empty());

    bson_destroy(document);
}

// Проверяется отказ при отсутствии массива термов или неверном типе поля.
TEST_F(BooleanIndexTest, RejectsMissingAndNonArrayTermFields) {
    bson_error_t error{};
    bson_t *missing = bson_new_from_json(
        reinterpret_cast<const uint8_t *>("{}"), -1, &error);
    ASSERT_NE(missing, nullptr) << error.message;
    bson_t *wrong_type = bson_new_from_json(
        reinterpret_cast<const uint8_t *>(
            "{\"lemmas\":\"database\"}"),
        -1, &error);
    ASSERT_NE(wrong_type, nullptr) << error.message;

    PostingBuffer buffer{};
    EXPECT_FALSE(add_document_terms(missing, "lemmas", 0, &buffer));
    EXPECT_FALSE(add_document_terms(wrong_type, "lemmas", 0, &buffer));
    EXPECT_TRUE(buffer.empty());

    bson_destroy(missing);
    bson_destroy(wrong_type);
}

// Проверяется пропуск пустого терма и сохранение технических обозначений.
TEST_F(BooleanIndexTest, SkipsEmptyTermAndPreservesTechnicalTerms) {
    bson_error_t error{};
    const char *json =
        "{\"lemmas\":[[\"\",9],[\"c++\",100],[\"node.js\",2],"
        "[\"sha-256\",1]]}";
    bson_t *document = bson_new_from_json(
        reinterpret_cast<const uint8_t *>(json), -1, &error);
    ASSERT_NE(document, nullptr) << error.message;

    PostingBuffer buffer{};
    ASSERT_TRUE(add_document_terms(document, "lemmas", 12, &buffer));
    ASSERT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer[0].term, "c++");
    EXPECT_EQ(buffer[1].term, "node.js");
    EXPECT_EQ(buffer[2].term, "sha-256");
    EXPECT_EQ(buffer[0].term_frequency, 100U);
    EXPECT_EQ(buffer[1].term_frequency, 2U);
    EXPECT_EQ(buffer[2].term_frequency, 1U);
    for (const PostingPair &pair : buffer) EXPECT_EQ(pair.document_id, 12U);

    bson_destroy(document);
}

// Проверяется отказ при нарушении структуры элемента массива термов.
TEST_F(BooleanIndexTest, RejectsMalformedTermArray) {
    bson_error_t error{};
    bson_t *document = bson_new_from_json(
        reinterpret_cast<const uint8_t *>("{\"lemmas\":[\"database\"]}"),
        -1, &error);
    ASSERT_NE(document, nullptr) << error.message;

    PostingBuffer buffer{};
    EXPECT_FALSE(add_document_terms(document, "lemmas", 0, &buffer));
    EXPECT_TRUE(buffer.empty());

    bson_destroy(document);
}

// Проверяется отказ при отсутствующей, нулевой, отрицательной или слишком большой TF.
TEST_F(BooleanIndexTest, RejectsInvalidTermFrequencies) {
    const char *invalid_documents[] = {
        "{\"lemmas\":[[\"database\"]]}",
        "{\"lemmas\":[[\"database\",\"3\"]]}",
        "{\"lemmas\":[[\"database\",0]]}",
        "{\"lemmas\":[[\"database\",-1]]}",
        "{\"lemmas\":[[\"database\",4294967296]]}",
    };

    for (const char *json : invalid_documents) {
        SCOPED_TRACE(json);
        bson_error_t error{};
        bson_t *document = bson_new_from_json(
            reinterpret_cast<const uint8_t *>(json), -1, &error);
        ASSERT_NE(document, nullptr) << error.message;
        PostingBuffer buffer{};
        EXPECT_FALSE(add_document_terms(document, "lemmas", 0, &buffer));
        EXPECT_TRUE(buffer.empty());
        bson_destroy(document);
    }
}

// Проверяется копирование в буфер ровно указанного диапазона символов терма.
TEST_F(BooleanIndexTest, PostingBufferCopiesExactlySpecifiedTermRange) {
    PostingBuffer buffer{};

    ASSERT_TRUE(posting_buffer_add(&buffer, "database-extra", 8, 42, 9));
    ASSERT_EQ(buffer.size(), 1U);
    EXPECT_EQ(buffer[0].term, "database");
    EXPECT_EQ(buffer[0].document_id, 42U);
    EXPECT_EQ(buffer[0].term_frequency, 9U);
}

// Проверяется отсутствие временного файла при пустом буфере posting.
TEST_F(BooleanIndexTest, DoesNotCreateTemporaryBlockForEmptyBuffer) {
    PostingBuffer buffer{};
    BlockList blocks{};

    flush_block(&buffer, &blocks, directory());

    EXPECT_TRUE(buffer.empty());
    EXPECT_TRUE(blocks.empty());
}

// Проверяется сортировка блока и удаление одинаковых пар терм–документ.
TEST_F(BooleanIndexTest, SortsTemporaryBlockAndRemovesDuplicatePairs) {
    PostingBuffer buffer{};
    BlockList blocks{};
    ASSERT_TRUE(posting_buffer_add(&buffer, "database", 8, 2, 4));
    ASSERT_TRUE(posting_buffer_add(&buffer, "cloud", 5, 1, 2));
    ASSERT_TRUE(posting_buffer_add(&buffer, "database", 8, 0, 3));
    ASSERT_TRUE(posting_buffer_add(&buffer, "cloud", 5, 1, 2));

    flush_block(&buffer, &blocks, directory());
    ASSERT_EQ(blocks.size(), 1U);
    FILE *file = std::fopen(blocks[0].c_str(), "rb");
    ASSERT_NE(file, nullptr);

    const char *expected_terms[] = {"cloud", "database", "database"};
    const uint32_t expected_documents[] = {1, 0, 2};
    const uint32_t expected_frequencies[] = {2, 3, 4};
    for (size_t index = 0; index < 3; ++index) {
        char term[64] = "";
        uint32_t document_id = UINT32_MAX;
        uint32_t term_frequency = 0;
        ASSERT_TRUE(read_raw_pair(file, term, sizeof(term), &document_id,
                                  &term_frequency));
        EXPECT_STREQ(term, expected_terms[index]);
        EXPECT_EQ(document_id, expected_documents[index]);
        EXPECT_EQ(term_frequency, expected_frequencies[index]);
    }
    char extra_term[64] = "";
    uint32_t extra_document = 0;
    uint32_t extra_frequency = 0;
    EXPECT_FALSE(read_raw_pair(file, extra_term, sizeof(extra_term),
                               &extra_document, &extra_frequency));

    std::fclose(file);
    destroy_blocks(&blocks);
}

void expect_postings(FILE *postings,
                     const boolean_index_format::LexiconEntry &entry,
                     const uint32_t *expected_documents,
                     const uint32_t *expected_frequencies,
                     size_t expected_size) {
    ASSERT_EQ(entry.postings_count, expected_size);
    ASSERT_EQ(fseeko(postings, static_cast<off_t>(entry.postings_offset), SEEK_SET),
              0);
    uint32_t document_id = 0;
    for (size_t index = 0; index < expected_size; ++index) {
        uint32_t document_gap = 0;
        uint32_t term_frequency = 0;
        ASSERT_TRUE(variable_byte_read(postings, &document_gap));
        ASSERT_TRUE(variable_byte_read(postings, &term_frequency));
        document_id += document_gap;
        EXPECT_EQ(document_id, expected_documents[index]);
        EXPECT_EQ(term_frequency, expected_frequencies[index]);
    }
}

// Проверяется слияние блоков в отсортированный инвертированный индекс.
TEST_F(BooleanIndexTest, MergesBlocksIntoSortedFinalInvertedIndex) {
    PostingBuffer first{};
    PostingBuffer second{};
    BlockList blocks{};

    ASSERT_TRUE(posting_buffer_add(&first, "database", 8, 0, 3));
    ASSERT_TRUE(posting_buffer_add(&first, "cloud", 5, 0, 4));
    ASSERT_TRUE(posting_buffer_add(&first, "security", 8, 2, 1));
    ASSERT_TRUE(posting_buffer_add(&first, "database", 8, 0, 3));
    flush_block(&first, &blocks, directory());

    ASSERT_TRUE(posting_buffer_add(&second, "database", 8, 3, 2));
    ASSERT_TRUE(posting_buffer_add(&second, "cloud", 5, 1, 6));
    ASSERT_TRUE(posting_buffer_add(&second, "security", 8, 2, 1));
    flush_block(&second, &blocks, directory());
    ASSERT_EQ(blocks.size(), 2U);

    merge_blocks(&blocks, directory(), 4);

    char path[PATH_SIZE];
    join_path(path, sizeof(path), directory(), "lexicon.bin");
    FILE *lexicon = std::fopen(path, "rb");
    join_path(path, sizeof(path), directory(), "terms.bin");
    FILE *terms = std::fopen(path, "rb");
    join_path(path, sizeof(path), directory(), "postings.bin");
    FILE *postings = std::fopen(path, "rb");
    ASSERT_NE(lexicon, nullptr);
    ASSERT_NE(terms, nullptr);
    ASSERT_NE(postings, nullptr);

    boolean_index_format::IndexHeader header{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, lexicon), 1U);
    EXPECT_EQ(header.document_count, 4U);
    EXPECT_EQ(header.term_count, 3U);
    EXPECT_EQ(header.posting_count, 5U);

    const char *expected_terms[] = {"cloud", "database", "security"};
    const uint32_t cloud[] = {0, 1};
    const uint32_t database[] = {0, 3};
    const uint32_t security[] = {2};
    const uint32_t *expected_postings[] = {cloud, database, security};
    const uint32_t cloud_frequencies[] = {4, 6};
    const uint32_t database_frequencies[] = {3, 2};
    const uint32_t security_frequencies[] = {1};
    const uint32_t *expected_frequencies[] = {
        cloud_frequencies, database_frequencies, security_frequencies};
    const size_t expected_sizes[] = {2, 2, 1};
    for (size_t index = 0; index < 3; ++index) {
        SCOPED_TRACE(expected_terms[index]);
        boolean_index_format::LexiconEntry entry{};
        ASSERT_EQ(std::fread(&entry, sizeof(entry), 1, lexicon), 1U);
        char term[64] = "";
        ASSERT_TRUE(read_term(terms, entry, term, sizeof(term)));
        EXPECT_STREQ(term, expected_terms[index]);
        expect_postings(postings, entry, expected_postings[index],
                        expected_frequencies[index],
                        expected_sizes[index]);
    }

    ASSERT_EQ(fseeko(postings, 0, SEEK_END), 0);
    EXPECT_EQ(ftello(postings), 10);
    EXPECT_LT(ftello(postings),
              static_cast<off_t>(header.posting_count * 2U * sizeof(uint32_t)));

    std::fclose(lexicon);
    std::fclose(terms);
    std::fclose(postings);
    destroy_blocks(&blocks);
}

// Проверяется создание корректных файлов индекса при отсутствии термов.
TEST_F(BooleanIndexTest, CreatesValidEmptyIndexWhenThereAreNoTerms) {
    BlockList blocks{};

    merge_blocks(&blocks, directory(), 3);

    char path[PATH_SIZE];
    join_path(path, sizeof(path), directory(), "lexicon.bin");
    FILE *lexicon = std::fopen(path, "rb");
    ASSERT_NE(lexicon, nullptr);
    boolean_index_format::IndexHeader header{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, lexicon), 1U);
    EXPECT_EQ(std::memcmp(header.magic,
                          boolean_index_format::INDEX_MAGIC, 8),
              0);
    EXPECT_EQ(header.document_count, 3U);
    EXPECT_EQ(header.term_count, 0U);
    EXPECT_EQ(header.posting_count, 0U);
    EXPECT_EQ(std::fgetc(lexicon), EOF);
    std::fclose(lexicon);

    join_path(path, sizeof(path), directory(), "terms.bin");
    FILE *terms = std::fopen(path, "rb");
    ASSERT_NE(terms, nullptr);
    EXPECT_EQ(std::fgetc(terms), EOF);
    std::fclose(terms);

    join_path(path, sizeof(path), directory(), "postings.bin");
    FILE *postings = std::fopen(path, "rb");
    ASSERT_NE(postings, nullptr);
    EXPECT_EQ(std::fgetc(postings), EOF);
    std::fclose(postings);
}

// Проверяется сохранение соответствия числового docID исходному ObjectId.
TEST_F(BooleanIndexTest, FinalizesDocumentIdToObjectIdTable) {
    char temporary_path[PATH_SIZE];
    join_path(temporary_path, sizeof(temporary_path), directory(),
              "documents.bin.tmp");
    FILE *file = std::fopen(temporary_path, "wb+");
    ASSERT_NE(file, nullptr);

    boolean_index_format::DocumentHeader placeholder{};
    ASSERT_EQ(std::fwrite(&placeholder, sizeof(placeholder), 1, file), 1U);
    uint8_t object_ids[2][12]{};
    object_ids[0][11] = 0x11;
    object_ids[1][11] = 0x22;
    ASSERT_EQ(std::fwrite(object_ids, sizeof(object_ids), 1, file), 1U);
    finalize_documents(file, temporary_path, directory(), 2);

    char final_path[PATH_SIZE];
    join_path(final_path, sizeof(final_path), directory(), "documents.bin");
    FILE *saved = std::fopen(final_path, "rb");
    ASSERT_NE(saved, nullptr);

    boolean_index_format::DocumentHeader header{};
    uint8_t actual[2][12]{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, saved), 1U);
    ASSERT_EQ(std::fread(actual, sizeof(actual), 1, saved), 1U);
    EXPECT_EQ(std::memcmp(header.magic,
                          boolean_index_format::DOCUMENT_MAGIC, 8),
              0);
    EXPECT_EQ(header.document_count, 2U);
    EXPECT_EQ(actual[0][11], 0x11);
    EXPECT_EQ(actual[1][11], 0x22);
    std::fclose(saved);
}

// Проверяется корректный заголовок таблицы без документов.
TEST_F(BooleanIndexTest, FinalizesEmptyDocumentTable) {
    char temporary_path[PATH_SIZE];
    join_path(temporary_path, sizeof(temporary_path), directory(),
              "documents.bin.tmp");
    FILE *file = std::fopen(temporary_path, "wb+");
    ASSERT_NE(file, nullptr);
    boolean_index_format::DocumentHeader placeholder{};
    ASSERT_EQ(std::fwrite(&placeholder, sizeof(placeholder), 1, file), 1U);

    finalize_documents(file, temporary_path, directory(), 0);

    char final_path[PATH_SIZE];
    join_path(final_path, sizeof(final_path), directory(), "documents.bin");
    FILE *saved = std::fopen(final_path, "rb");
    ASSERT_NE(saved, nullptr);
    boolean_index_format::DocumentHeader header{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, saved), 1U);
    EXPECT_EQ(std::memcmp(header.magic,
                          boolean_index_format::DOCUMENT_MAGIC, 8),
              0);
    EXPECT_EQ(header.document_count, 0U);
    EXPECT_EQ(std::fgetc(saved), EOF);
    std::fclose(saved);
}

// Проверяется заголовок и значения таблицы L2-норм документов для TF-IDF.
TEST_F(BooleanIndexTest, FinalizesDocumentNormTable) {
    char temporary_path[PATH_SIZE];
    join_path(temporary_path, sizeof(temporary_path), directory(),
              "norms.bin.tmp");
    FILE *file = std::fopen(temporary_path, "wb+");
    ASSERT_NE(file, nullptr);
    boolean_index_format::DocumentNormHeader placeholder{};
    ASSERT_EQ(std::fwrite(&placeholder, sizeof(placeholder), 1, file), 1U);
    const double norms[] = {1.0, 2.5, 0.0};
    ASSERT_EQ(std::fwrite(norms, sizeof(norms), 1, file), 1U);

    finalize_document_norms(file, temporary_path, directory(), 3);

    char final_path[PATH_SIZE];
    join_path(final_path, sizeof(final_path), directory(), "norms.bin");
    FILE *saved = std::fopen(final_path, "rb");
    ASSERT_NE(saved, nullptr);
    boolean_index_format::DocumentNormHeader header{};
    double actual[3]{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, saved), 1U);
    ASSERT_EQ(std::fread(actual, sizeof(actual), 1, saved), 1U);
    EXPECT_EQ(std::memcmp(header.magic,
                          boolean_index_format::DOCUMENT_NORM_MAGIC, 8),
              0);
    EXPECT_EQ(header.version, boolean_index_format::VERSION);
    EXPECT_EQ(header.document_count, 3U);
    EXPECT_DOUBLE_EQ(actual[0], 1.0);
    EXPECT_DOUBLE_EQ(actual[1], 2.5);
    EXPECT_DOUBLE_EQ(actual[2], 0.0);
    std::fclose(saved);
}

}  // namespace
