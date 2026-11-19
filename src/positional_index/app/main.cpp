// Запускает построение координатного индекса для цитатного поиска.
#include "positional_index/app/config.hpp"
#include "positional_index/building/builder.hpp"

#include <mongoc/mongoc.h>

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr,
                     "Использование: %s configs/positional_index.yaml\n",
                     argv[0]);
        return EXIT_FAILURE;
    }
    const PositionalIndexConfig config =
        load_positional_index_config(argv[1]);
    mongoc_init();
    build_positional_index(config);
    mongoc_cleanup();
    return EXIT_SUCCESS;
}
