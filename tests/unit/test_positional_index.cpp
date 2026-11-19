#include <gtest/gtest.h>

#include "boolean_index/storage/files.hpp"
#include "phrase_search/index/index_reader.hpp"
#include "positional_index/building/blocks.hpp"
#include "positional_index/storage/format.hpp"
#include "positional_index/storage/writer.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

namespace {

void remove_files(const char *directory) {
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

class PositionalIndexTest : public testing::Test {
protected:
    void SetUp() override {
        std::snprintf(directory_, sizeof(directory_),
                      "/tmp/mai-ir-position-index-XXXXXX");
        ASSERT_NE(mkdtemp(directory_), nullptr);
    }

    void TearDown() override {
        remove_files(directory_);
        rmdir(directory_);
    }

    const char *directory() const { return directory_; }

private:
    char directory_[128]{};
};

}  // namespace

// Проверяется фиксированный размер заголовка и записи координатного лексикона.
TEST(PositionalIndexFormatTest, HasStablePackedLayout) {
    EXPECT_EQ(sizeof(positional_index_format::Header), 40U);
    EXPECT_EQ(sizeof(positional_index_format::LexiconEntry), 32U);
    EXPECT_EQ(positional_index_format::VERSION, 1U);
    EXPECT_EQ(std::memcmp(positional_index_format::MAGIC, "MAIIRPOS", 8), 0);
}

// Проверяется сортировка вхождений сначала по терму, затем по docID и позиции.
TEST_F(PositionalIndexTest, SortsOccurrenceBlock) {
    PositionBuffer buffer;
    position_buffer_add(&buffer, "security", 2, 8);
    position_buffer_add(&buffer, "cloud", 3, 7);
    position_buffer_add(&buffer, "cloud", 1, 9);
    position_buffer_add(&buffer, "cloud", 1, 2);
    PositionBlockList blocks;

    flush_position_block(&buffer, &blocks, directory());

    ASSERT_EQ(blocks.size(), 1U);
    FILE *file = std::fopen(blocks[0].c_str(), "rb");
    ASSERT_NE(file, nullptr);
    PositionBlockReader reader{};
    reader.file = file;
    ASSERT_TRUE(position_block_reader_next(&reader));
    EXPECT_EQ(reader.term, "cloud");
    EXPECT_EQ(reader.document_id, 1U);
    EXPECT_EQ(reader.position, 2U);
    ASSERT_TRUE(position_block_reader_next(&reader));
    EXPECT_EQ(reader.term, "cloud");
    EXPECT_EQ(reader.document_id, 1U);
    EXPECT_EQ(reader.position, 9U);
    ASSERT_TRUE(position_block_reader_next(&reader));
    EXPECT_EQ(reader.term, "cloud");
    EXPECT_EQ(reader.document_id, 3U);
    ASSERT_TRUE(position_block_reader_next(&reader));
    EXPECT_EQ(reader.term, "security");
    EXPECT_EQ(reader.document_id, 2U);
    ASSERT_TRUE(position_block_reader_next(&reader));
    EXPECT_FALSE(reader.active);
    std::fclose(file);
}

// Проверяется слияние нескольких блоков и сохранение всех координат posting list.
TEST_F(PositionalIndexTest, WritesCompressedCoordinatePostingLists) {
    PositionBlockList blocks;
    PositionBuffer first;
    position_buffer_add(&first, "cloud", 0, 0);
    position_buffer_add(&first, "cloud", 0, 4);
    position_buffer_add(&first, "database", 1, 2);
    flush_position_block(&first, &blocks, directory());
    PositionBuffer second;
    position_buffer_add(&second, "cloud", 2, 7);
    position_buffer_add(&second, "database", 2, 1);
    flush_position_block(&second, &blocks, directory());

    merge_position_blocks(&blocks, directory(), 3);

    char path[PATH_SIZE];
    join_path(path, sizeof(path), directory(), "coordinate_lexicon.bin");
    FILE *lexicon = std::fopen(path, "rb");
    ASSERT_NE(lexicon, nullptr);
    positional_index_format::Header header{};
    ASSERT_EQ(std::fread(&header, sizeof(header), 1, lexicon), 1U);
    EXPECT_EQ(header.document_count, 3U);
    EXPECT_EQ(header.term_count, 2U);
    EXPECT_EQ(header.posting_count, 4U);
    EXPECT_EQ(header.position_count, 5U);
    positional_index_format::LexiconEntry cloud{};
    ASSERT_EQ(std::fread(&cloud, sizeof(cloud), 1, lexicon), 1U);
    EXPECT_EQ(cloud.postings_count, 2U);
    EXPECT_EQ(cloud.position_count, 3U);
    std::fclose(lexicon);
}
