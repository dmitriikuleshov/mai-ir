#ifndef MAI_IR_BOOLEAN_INDEX_FILES_HPP
#define MAI_IR_BOOLEAN_INDEX_FILES_HPP

#include <cstddef>

constexpr size_t PATH_SIZE = 4096;

void make_directories(const char *path);
void join_path(char *target, size_t target_size, const char *directory,
               const char *name);

#endif
