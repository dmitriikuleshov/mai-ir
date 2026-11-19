// Объявляет блоки внешней сортировки отдельных вхождений термов.
#ifndef MAI_IR_POSITIONAL_INDEX_BLOCKS_HPP
#define MAI_IR_POSITIONAL_INDEX_BLOCKS_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct PositionOccurrence {
    std::string term;
    uint32_t document_id;
    uint32_t position;
};

using PositionBuffer = std::vector<PositionOccurrence>;
using PositionBlockList = std::vector<std::string>;

struct PositionBlockReader {
    FILE *file = nullptr;
    std::string term;
    uint32_t document_id = 0;
    uint32_t position = 0;
    bool active = false;
};

struct PositionReaderHeap {
    std::vector<size_t> items;
    std::vector<PositionBlockReader> *readers;
};

void position_buffer_add(PositionBuffer *buffer, const std::string &term,
                         uint32_t document_id, uint32_t position);
void flush_position_block(PositionBuffer *buffer, PositionBlockList *blocks,
                          const char *temporary_directory);
void reduce_position_blocks(PositionBlockList *blocks,
                            const char *temporary_directory);
bool position_block_reader_next(PositionBlockReader *reader);
void position_heap_sift_down(PositionReaderHeap *heap, size_t position);

#endif
