// Объявляет кодирование и файловый ввод-вывод целых чисел Variable Byte.
#ifndef MAI_IR_BOOLEAN_INDEX_VARIABLE_BYTE_HPP
#define MAI_IR_BOOLEAN_INDEX_VARIABLE_BYTE_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>

// Для uint32_t достаточно пяти групп по семь значащих бит.
constexpr size_t VARIABLE_BYTE_MAX_SIZE = 5;

// Кодирует число в переданный буфер и возвращает фактический размер.
size_t variable_byte_encode(uint32_t value,
                            uint8_t output[VARIABLE_BYTE_MAX_SIZE]);

// Декодирует число из памяти и возвращает количество использованных байтов.
bool variable_byte_decode(const uint8_t *data, size_t size,
                          size_t *consumed, uint32_t *value);

// Записывает одно Variable Byte-число в бинарный файл.
bool variable_byte_write(FILE *file, uint32_t value);

// Читает одно Variable Byte-число из бинарного файла.
bool variable_byte_read(FILE *file, uint32_t *value);

#endif
