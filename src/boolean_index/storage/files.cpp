// Содержит небольшие безопасные операции с каталогами и путями файлов индекса.
#include "boolean_index/storage/files.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace {

// Завершает индексатор при невозможности подготовить путь результата.
[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

}  // namespace

// Рекурсивно создаёт все отсутствующие компоненты указанного пути.
void make_directories(const char *path) {
    std::string buffer(path);
    for (size_t position = 1; position < buffer.size(); ++position) {
        if (buffer[position] == '/') {
            buffer[position] = '\0';
            if (mkdir(buffer.c_str(), 0755) != 0 && errno != EEXIST) {
                fail("не удалось создать каталог индекса");
            }
            buffer[position] = '/';
        }
    }
    if (mkdir(buffer.c_str(), 0755) != 0 && errno != EEXIST) {
        fail("не удалось создать каталог индекса");
    }
}

// Соединяет каталог и имя файла с предварительной проверкой размера буфера.
void join_path(char *target, size_t target_size, const char *directory,
               const char *name) {
    const size_t directory_size = std::strlen(directory);
    const size_t name_size = std::strlen(name);
    if (directory_size + name_size + 2 > target_size) {
        fail("слишком длинный путь файла индекса");
    }
    std::memcpy(target, directory, directory_size);
    target[directory_size] = '/';
    std::memcpy(target + directory_size + 1, name, name_size + 1);
}
