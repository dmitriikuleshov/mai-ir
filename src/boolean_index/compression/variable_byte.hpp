#ifndef MAI_IR_BOOLEAN_INDEX_VARIABLE_BYTE_HPP
#define MAI_IR_BOOLEAN_INDEX_VARIABLE_BYTE_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr size_t VARIABLE_BYTE_MAX_SIZE = 5;

size_t variable_byte_encode(uint32_t value,
                            uint8_t output[VARIABLE_BYTE_MAX_SIZE]);
bool variable_byte_decode(const uint8_t *data, size_t size,
                          size_t *consumed, uint32_t *value);

bool variable_byte_write(FILE *file, uint32_t value);
bool variable_byte_read(FILE *file, uint32_t *value);

#endif
