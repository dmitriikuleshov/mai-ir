#include "boolean_search/app/config.hpp"
#include "boolean_search/app/console.hpp"
#include "boolean_search/index/index_reader.hpp"

#include <mongoc/mongoc.h>

#include <cstdio>
#include <cstdlib>

namespace {

[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s config.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    const BooleanSearchConfig config = load_boolean_search_config(argv[1]);

    BooleanIndex index{};
    char error[512] = "";
    if (!boolean_index_open(&index, config.index_directory.c_str(), error,
                            sizeof(error))) {
        fail(error);
    }
    LemmaDictionary *dictionary = lemma_dictionary_load(
        config.lemma_dictionary.c_str(), config.lemma_vocabulary.c_str(),
        config.lemma_overrides.c_str());
    if (dictionary == nullptr) {
        boolean_index_close(&index);
        fail("не удалось загрузить словарь лемматизации");
    }

    mongoc_init();
    mongoc_client_t *client = mongoc_client_new(config.uri.c_str());
    if (client == nullptr) {
        lemma_dictionary_destroy(dictionary);
        boolean_index_close(&index);
        fail("не удалось создать клиент MongoDB");
    }
    mongoc_collection_t *collection = mongoc_client_get_collection(
        client, config.database.c_str(), config.processed_collection.c_str());

    run_boolean_search_console(config, &index, dictionary, collection);

    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_cleanup();
    lemma_dictionary_destroy(dictionary);
    boolean_index_close(&index);
    return EXIT_SUCCESS;
}
