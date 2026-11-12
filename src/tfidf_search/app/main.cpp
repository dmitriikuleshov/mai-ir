// Собирает индекс, словари и консольный интерфейс в утилиту TF-IDF-поиска.
#include "boolean_search/app/config.hpp"
#include "tfidf_search/app/console.hpp"
#include "tfidf_search/index/ranking_index.hpp"

#include <mongoc/mongoc.h>

#include <cstdio>
#include <cstdlib>

namespace {

// Завершает запуск с понятным сообщением при отсутствии обязательного ресурса.
[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

}  // namespace

// Загружает конфигурацию и индекс, запускает консоль, затем освобождает ресурсы.
int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s config.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    const BooleanSearchConfig config = load_boolean_search_config(argv[1]);

    TfidfIndex index{};
    char error[512] = "";
    if (!tfidf_index_open(&index, config.index_directory.c_str(), error,
                          sizeof(error))) {
        fail(error);
    }
    LemmaDictionary *dictionary = lemma_dictionary_load(
        config.lemma_dictionary.c_str(), config.lemma_vocabulary.c_str(),
        config.lemma_overrides.c_str());
    if (dictionary == nullptr) {
        tfidf_index_close(&index);
        fail("не удалось загрузить словарь лемматизации");
    }

    mongoc_init();
    mongoc_client_t *client = mongoc_client_new(config.uri.c_str());
    if (client == nullptr) {
        lemma_dictionary_destroy(dictionary);
        tfidf_index_close(&index);
        fail("не удалось создать клиент MongoDB");
    }
    mongoc_collection_t *collection = mongoc_client_get_collection(
        client, config.database.c_str(), config.processed_collection.c_str());

    run_tfidf_search_console(config, &index, dictionary, collection);

    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_cleanup();
    lemma_dictionary_destroy(dictionary);
    tfidf_index_close(&index);
    return EXIT_SUCCESS;
}
