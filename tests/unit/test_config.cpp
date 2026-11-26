#include <gtest/gtest.h>

#include "linguistics/app/config.hpp"

namespace {

constexpr const char *CONFIG_PATH =
    MAI_IR_SOURCE_DIR "/configs/linguistics.yaml";

class ConfigTest : public testing::Test {
protected:
    void SetUp() override { config_ = load_config(CONFIG_PATH); }

    Config config_{};
};

// Проверяется чтение параметров MongoDB и лингвистической обработки из YAML.
TEST_F(ConfigTest, ReadsDatabaseAndLinguisticsSettings) {
    EXPECT_EQ(config_.database, "mai_ir");
    EXPECT_EQ(config_.documents, "documents");
    EXPECT_EQ(config_.minimum_words, 100U);
    EXPECT_EQ(config_.parser_version, 9U);
}

// Проверяется разрешение относительных путей от каталога YAML-конфигурации.
TEST_F(ConfigTest, ResolvesFilePathsRelativeToConfigLocation) {
    EXPECT_EQ(config_.lemma_dictionary,
              MAI_IR_SOURCE_DIR
              "/configs/../resources/wordnet_exceptions.tsv");
    EXPECT_EQ(config_.lemma_vocabulary,
              MAI_IR_SOURCE_DIR "/configs/../resources/wordnet_lemmas.txt");
    EXPECT_EQ(config_.lemma_overrides,
              MAI_IR_SOURCE_DIR
              "/configs/../resources/technical_lemma_overrides.tsv");
    EXPECT_EQ(config_.output_directory,
              MAI_IR_SOURCE_DIR "/configs/../results");
}

}  // namespace
