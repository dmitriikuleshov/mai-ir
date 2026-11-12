// Объявляет структуры временных posting-блоков и операции их внешней сортировки.
#ifndef MAI_IR_BOOLEAN_INDEX_BLOCKS_HPP
#define MAI_IR_BOOLEAN_INDEX_BLOCKS_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct PostingPair {
    std::string term;
    uint32_t document_id;
    uint32_t term_frequency;
};

// Буфер хранится только до заданного лимита, а список блоков содержит пути на диске.
using PostingBuffer = std::vector<PostingPair>;
using BlockList = std::vector<std::string>;

// Хранит текущую запись и состояние последовательного чтения одного блока.
struct BlockReader {
    FILE *file = nullptr;
    std::string term;
    uint32_t document_id = 0;
    uint32_t term_frequency = 0;
    bool active = false;
};

// Минимальная куча хранит индексы читателей, а не копии их строк.
struct ReaderHeap {
    std::vector<size_t> items;
    std::vector<BlockReader> *readers;
};

// Добавляет одну пару «терм — документ» вместе с частотой в буфер.
bool posting_buffer_add(PostingBuffer *buffer, const char *term,
                        size_t term_size, uint32_t document_id,
                        uint32_t term_frequency);

// Записывает накопленный буфер как отсортированный временный файл.
void flush_block(PostingBuffer *buffer, BlockList *blocks,
                 const char *temporary_directory);

// Уменьшает число временных блоков до безопасного для финального слияния.
void reduce_blocks(BlockList *blocks, const char *temporary_directory);

// Читает следующую запись из временного блока.
bool block_reader_next(BlockReader *reader);

// Опускает элемент кучи читателей на правильную позицию.
void heap_sift_down(ReaderHeap *heap, size_t position);

#endif
