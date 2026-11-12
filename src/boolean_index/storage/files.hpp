// Объявляет операции подготовки каталогов и путей бинарного индекса.
#ifndef MAI_IR_BOOLEAN_INDEX_FILES_HPP
#define MAI_IR_BOOLEAN_INDEX_FILES_HPP

#include <cstddef>

constexpr size_t PATH_SIZE = 4096;

// Создаёт конечный каталог вместе со всеми отсутствующими родителями.
void make_directories(const char *path);

// Формирует путь «каталог/имя» в буфере фиксированного размера.
void join_path(char *target, size_t target_size, const char *directory,
               const char *name);

#endif
