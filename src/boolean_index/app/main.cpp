// Запускает построение индекса по указанному файлу конфигурации.
#include "boolean_index/app/config.hpp"
#include "boolean_index/building/builder.hpp"

#include <mongoc/mongoc.h>

#include <cstdio>
#include <cstdlib>

// Проверяет аргументы, инициализирует MongoDB-драйвер и запускает индексатор.
int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s config.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    const BooleanIndexConfig config = load_boolean_index_config(argv[1]);
    mongoc_init();
    build_boolean_index(config);
    mongoc_cleanup();
    return EXIT_SUCCESS;
}
