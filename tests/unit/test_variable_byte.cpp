#include <gtest/gtest.h>

#include "boolean_index/compression/variable_byte.hpp"

#include <cstdint>
#include <cstdio>

namespace {

struct EncodingExample {
    uint32_t value;
    uint8_t bytes[VARIABLE_BYTE_MAX_SIZE];
    size_t size;
};

// Проверяются точные байтовые представления на границах разрядов VB-кода.
TEST(VariableByteTest, EncodesBoundaryValuesExactly) {
    const EncodingExample examples[] = {
        {0, {0x80}, 1},
        {1, {0x81}, 1},
        {127, {0xFF}, 1},
        {128, {0x01, 0x80}, 2},
        {129, {0x01, 0x81}, 2},
        {16383, {0x7F, 0xFF}, 2},
        {16384, {0x01, 0x00, 0x80}, 3},
        {UINT32_MAX, {0x0F, 0x7F, 0x7F, 0x7F, 0xFF}, 5},
    };

    for (const EncodingExample &example : examples) {
        SCOPED_TRACE(example.value);
        uint8_t actual[VARIABLE_BYTE_MAX_SIZE]{};
        const size_t size = variable_byte_encode(example.value, actual);
        ASSERT_EQ(size, example.size);
        for (size_t index = 0; index < size; ++index) {
            EXPECT_EQ(actual[index], example.bytes[index]);
        }
    }
}

// Проверяется обратимость кодирования для малых, граничных и больших чисел.
TEST(VariableByteTest, DecodesEveryEncodedValueBack) {
    const uint32_t values[] = {0,       1,       2,         126,
                               127,     128,     255,       16383,
                               16384,   1U << 21U, 1U << 28U, UINT32_MAX};

    for (uint32_t expected : values) {
        SCOPED_TRACE(expected);
        uint8_t encoded[VARIABLE_BYTE_MAX_SIZE]{};
        const size_t size = variable_byte_encode(expected, encoded);
        size_t consumed = 0;
        uint32_t actual = 0;
        ASSERT_TRUE(variable_byte_decode(encoded, size, &consumed, &actual));
        EXPECT_EQ(consumed, size);
        EXPECT_EQ(actual, expected);
    }
}

// Проверяется последовательная запись и чтение нескольких чисел из файла.
TEST(VariableByteTest, ReadsAndWritesASequenceFromFile) {
    FILE *file = std::tmpfile();
    ASSERT_NE(file, nullptr);
    const uint32_t values[] = {0, 3, 127, 128, 50000, UINT32_MAX};
    for (uint32_t value : values) ASSERT_TRUE(variable_byte_write(file, value));
    ASSERT_EQ(std::fflush(file), 0);
    ASSERT_EQ(fseeko(file, 0, SEEK_SET), 0);

    for (uint32_t expected : values) {
        uint32_t actual = 0;
        ASSERT_TRUE(variable_byte_read(file, &actual));
        EXPECT_EQ(actual, expected);
    }
    uint32_t extra = 0;
    EXPECT_FALSE(variable_byte_read(file, &extra));
    std::fclose(file);
}

// Проверяется отказ при отсутствии завершающего байта числа.
TEST(VariableByteTest, RejectsTruncatedEncoding) {
    const uint8_t truncated[] = {0x01, 0x02};
    size_t consumed = 0;
    uint32_t value = 0;

    EXPECT_FALSE(variable_byte_decode(truncated, sizeof(truncated), &consumed,
                                      &value));
}

// Проверяется отказ, если закодированное число не помещается в uint32_t.
TEST(VariableByteTest, RejectsUint32Overflow) {
    const uint8_t overflow[] = {0x7F, 0x7F, 0x7F, 0x7F, 0xFF};
    size_t consumed = 0;
    uint32_t value = 0;

    EXPECT_FALSE(variable_byte_decode(overflow, sizeof(overflow), &consumed,
                                      &value));
}

// Проверяется остановка декодера на конце первого числа в общем буфере.
TEST(VariableByteTest, ReportsConsumedBytesForFirstValue) {
    const uint8_t data[] = {0x01, 0x80, 0x85};
    size_t consumed = 0;
    uint32_t value = 0;

    ASSERT_TRUE(variable_byte_decode(data, sizeof(data), &consumed, &value));
    EXPECT_EQ(value, 128U);
    EXPECT_EQ(consumed, 2U);
}

}  // namespace
