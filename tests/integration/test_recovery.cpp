#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

namespace {

long find_line(const char *path, const char *needle) {
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) return -1;

    char line[4096];
    long line_number = 0;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        ++line_number;
        if (std::strstr(line, needle) != nullptr) {
            std::fclose(file);
            return line_number;
        }
    }
    std::fclose(file);
    return -1;
}

// Проверяется запись commit marker только после сохранения термов документа.
TEST(RecoveryProtocolTest, CommitsProcessedDocumentOnlyAfterTermsAreStored) {
    constexpr const char *SOURCE =
        MAI_IR_SOURCE_DIR "/src/linguistics/processing/document_processor.cpp";
    const long terms = find_line(
        SOURCE, "execute_bulk(terms_bulk, \"запись термов документов\")");
    const long processed = find_line(
        SOURCE,
        "execute_bulk(processed_bulk, \"запись обработанных документов\")");

    ASSERT_GT(terms, 0) << "Не найдена запись document_terms";
    ASSERT_GT(processed, 0) << "Не найдена запись processed_documents";
    EXPECT_GT(processed, terms)
        << "processed_documents должен быть commit marker";
}

}  // namespace
