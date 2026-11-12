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

using PostingBuffer = std::vector<PostingPair>;
using BlockList = std::vector<std::string>;

struct BlockReader {
    FILE *file = nullptr;
    std::string term;
    uint32_t document_id = 0;
    uint32_t term_frequency = 0;
    bool active = false;
};

struct ReaderHeap {
    std::vector<size_t> items;
    std::vector<BlockReader> *readers;
};

bool posting_buffer_add(PostingBuffer *buffer, const char *term,
                        size_t term_size, uint32_t document_id,
                        uint32_t term_frequency);
void flush_block(PostingBuffer *buffer, BlockList *blocks,
                 const char *temporary_directory);
void reduce_blocks(BlockList *blocks, const char *temporary_directory);

bool block_reader_next(BlockReader *reader);
void heap_sift_down(ReaderHeap *heap, size_t position);

#endif
