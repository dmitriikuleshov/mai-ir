#include "engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void path_join(char *target, size_t capacity, const char *directory,
               const char *name) {
    require(std::snprintf(target, capacity, "%s/%s", directory, name) <
                static_cast<int>(capacity),
            "test path is too long");
}

void write_test_index(const char *directory) {
    const char *terms[] = {"cloud", "database", "security"};
    const uint32_t lists[][2] = {{0, 1}, {1, 2}, {2, 3}};
    char path[4096];

    path_join(path, sizeof(path), directory, "terms.bin");
    FILE *terms_file = std::fopen(path, "wb");
    require(terms_file != nullptr, "open terms");
    for (const char *term : terms) {
        require(std::fwrite(term, 1, std::strlen(term), terms_file) ==
                    std::strlen(term),
                "write terms");
    }
    std::fclose(terms_file);

    path_join(path, sizeof(path), directory, "postings.bin");
    FILE *postings = std::fopen(path, "wb");
    require(postings != nullptr, "open postings");
    require(std::fwrite(lists, sizeof(lists), 1, postings) == 1,
            "write postings");
    std::fclose(postings);

    path_join(path, sizeof(path), directory, "lexicon.bin");
    FILE *lexicon = std::fopen(path, "wb");
    require(lexicon != nullptr, "open lexicon");
    boolean_index_format::IndexHeader header{};
    std::memcpy(header.magic, boolean_index_format::INDEX_MAGIC,
                sizeof(header.magic));
    header.version = boolean_index_format::VERSION;
    header.document_count = 4;
    header.term_count = 3;
    header.posting_count = 6;
    require(std::fwrite(&header, sizeof(header), 1, lexicon) == 1,
            "write header");
    uint64_t term_offset = 0;
    for (size_t index = 0; index < 3; ++index) {
        const boolean_index_format::LexiconEntry entry{
            term_offset, index * 2 * sizeof(uint32_t),
            static_cast<uint32_t>(std::strlen(terms[index])), 2};
        require(std::fwrite(&entry, sizeof(entry), 1, lexicon) == 1,
                "write lexicon entry");
        term_offset += std::strlen(terms[index]);
    }
    std::fclose(lexicon);

    path_join(path, sizeof(path), directory, "documents.bin");
    FILE *documents = std::fopen(path, "wb");
    require(documents != nullptr, "open documents");
    boolean_index_format::DocumentHeader document_header{};
    std::memcpy(document_header.magic, boolean_index_format::DOCUMENT_MAGIC,
                sizeof(document_header.magic));
    document_header.version = boolean_index_format::VERSION;
    document_header.document_count = 4;
    require(std::fwrite(&document_header, sizeof(document_header), 1,
                        documents) == 1,
            "write document header");
    uint8_t oids[4][12]{};
    for (size_t index = 0; index < 4; ++index) {
        oids[index][11] = static_cast<uint8_t>(index + 1);
    }
    require(std::fwrite(oids, sizeof(oids), 1, documents) == 1,
            "write object ids");
    std::fclose(documents);
}

void expect_query(BooleanIndex *index, const LemmaDictionary *dictionary,
                  const char *query, const uint32_t *expected,
                  size_t expected_size) {
    char error[256] = "";
    BooleanResult result = boolean_search(index, query, 100, dictionary,
                                          error, sizeof(error));
    require(error[0] == '\0', error);
    require(result.size == expected_size, "unexpected result size");
    for (size_t position = 0; position < expected_size; ++position) {
        require(result.document_ids[position] == expected[position],
                "unexpected document id");
    }
    boolean_result_destroy(&result);
}

}  // namespace

int main() {
    char directory[] = "/tmp/mai-ir-search-XXXXXX";
    require(mkdtemp(directory) != nullptr, "create temporary directory");
    write_test_index(directory);

    BooleanIndex index{};
    char error[256] = "";
    require(boolean_index_open(&index, directory, error, sizeof(error)), error);
    LemmaDictionary *dictionary =
        lemma_dictionary_load("resources/wordnet_exceptions.tsv");
    require(dictionary != nullptr, "load lemma dictionary");

    const uint32_t cloud[] = {0, 1};
    const uint32_t intersection[] = {1};
    const uint32_t all[] = {0, 1, 2, 3};
    const uint32_t complement[] = {2, 3};
    const uint32_t precedence[] = {0, 1, 2};
    const uint32_t parentheses[] = {2};
    expect_query(&index, dictionary, "cloud", cloud, 2);
    expect_query(&index, dictionary, "cloud AND database", intersection, 1);
    expect_query(&index, dictionary, "cloud OR security", all, 4);
    expect_query(&index, dictionary, "NOT cloud", complement, 2);
    expect_query(&index, dictionary, "cloud OR database AND security",
                 precedence, 3);
    expect_query(&index, dictionary, "(cloud OR database) AND security",
                 parentheses, 1);

    BooleanResult invalid = boolean_search(&index, "cloud AND", 100,
                                           dictionary, error, sizeof(error));
    require(error[0] != '\0' && invalid.size == 0,
            "invalid query must return an error");
    boolean_result_destroy(&invalid);

    lemma_dictionary_destroy(dictionary);
    boolean_index_close(&index);
    const char *files[] = {"lexicon.bin", "terms.bin", "postings.bin",
                           "documents.bin"};
    for (const char *name : files) {
        char path[4096];
        path_join(path, sizeof(path), directory, name);
        unlink(path);
    }
    rmdir(directory);
    std::puts("PASS: boolean index and query operations");
    return EXIT_SUCCESS;
}
