#include "linguistics/app/application.hpp"
#include "linguistics/app/config.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s config.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    return run_linguistics(load_config(argv[1]));
}
