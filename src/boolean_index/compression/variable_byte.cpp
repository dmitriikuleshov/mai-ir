// Реализует Variable Byte для компактного хранения docID-gap и частот терминов.
#include "boolean_index/compression/variable_byte.hpp"

#include <climits>

// Кодирует 32-битное число группами по 7 значащих бит.
size_t variable_byte_encode(uint32_t value,
                            uint8_t output[VARIABLE_BYTE_MAX_SIZE]) {
    uint8_t reversed[VARIABLE_BYTE_MAX_SIZE];
    size_t size = 0;
    do {
        reversed[size++] = static_cast<uint8_t>(value & 0x7FU);
        value >>= 7U;
    } while (value != 0);

    // Группы разворачиваются от старших к младшим, а старший бит завершает число.
    for (size_t index = 0; index < size; ++index) {
        output[index] = reversed[size - index - 1];
    }
    output[size - 1] |= 0x80U;
    return size;
}

// Декодирует одно Variable Byte-число и сообщает число использованных байтов.
bool variable_byte_decode(const uint8_t *data, size_t size,
                          size_t *consumed, uint32_t *value) {
    uint32_t result = 0;
    const size_t limit = size < VARIABLE_BYTE_MAX_SIZE
                             ? size
                             : VARIABLE_BYTE_MAX_SIZE;
    for (size_t index = 0; index < limit; ++index) {
        const uint32_t payload = data[index] & 0x7FU;
        // Проверка выполняется до умножения, чтобы исключить переполнение uint32_t.
        if (result > (UINT32_MAX - payload) / 128U) {
            return false;
        }
        result = result * 128U + payload;
        if ((data[index] & 0x80U) != 0) {
            *consumed = index + 1;
            *value = result;
            return true;
        }
    }
    return false;
}

// Кодирует число и полностью записывает его в открытый бинарный файл.
bool variable_byte_write(FILE *file, uint32_t value) {
    uint8_t encoded[VARIABLE_BYTE_MAX_SIZE];
    const size_t size = variable_byte_encode(value, encoded);
    return std::fwrite(encoded, 1, size, file) == size;
}

// Читает байты до маркера конца и декодирует одно число из файла.
bool variable_byte_read(FILE *file, uint32_t *value) {
    uint8_t encoded[VARIABLE_BYTE_MAX_SIZE];
    for (size_t size = 0; size < VARIABLE_BYTE_MAX_SIZE; ++size) {
        if (std::fread(&encoded[size], 1, 1, file) != 1) {
            return false;
        }
        if ((encoded[size] & 0x80U) != 0) {
            size_t consumed = 0;
            return variable_byte_decode(encoded, size + 1, &consumed, value) &&
                   consumed == size + 1;
        }
    }
    return false;
}
