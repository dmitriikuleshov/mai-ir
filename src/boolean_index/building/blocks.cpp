#include "boolean_index/building/blocks.hpp"

#include "boolean_index/storage/files.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <unistd.h>

namespace {

constexpr size_t MERGE_FAN_IN = 128;

[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

bool posting_pair_before(const PostingPair &left, const PostingPair &right) {
    return left.term < right.term ||
           (left.term == right.term && left.document_id < right.document_id);
}

bool posting_pairs_equal(const PostingPair &left, const PostingPair &right) {
    return left.term == right.term && left.document_id == right.document_id;
}

bool reader_before(const ReaderHeap *heap, size_t first, size_t second) {
    const BlockReader &left = (*heap->readers)[first];
    const BlockReader &right = (*heap->readers)[second];
    return left.term < right.term ||
           (left.term == right.term && left.document_id < right.document_id) ||
           (left.term == right.term && left.document_id == right.document_id &&
            first < second);
}

void merge_raw_group(const BlockList &input_paths, size_t begin,
                     size_t input_count, const char *output_path) {
    FILE *output = std::fopen(output_path, "wb");
    if (output == nullptr) {
        fail("не удалось подготовить промежуточное слияние");
    }
    std::vector<BlockReader> readers(input_count);
    ReaderHeap heap{{}, &readers};
    heap.items.reserve(input_count);
    for (size_t index = 0; index < input_count; ++index) {
        readers[index].file = std::fopen(input_paths[begin + index].c_str(), "rb");
        if (readers[index].file == nullptr || !block_reader_next(&readers[index])) {
            fail("не удалось прочитать блок при промежуточном слиянии");
        }
        if (readers[index].active) {
            heap.items.push_back(index);
        }
    }
    for (size_t position = heap.items.size() / 2; position > 0; --position) {
        heap_sift_down(&heap, position - 1);
    }

    std::string previous_term;
    uint32_t previous_document = 0;
    bool has_previous = false;
    while (!heap.items.empty()) {
        const size_t reader_index = heap.items[0];
        BlockReader *reader = &readers[reader_index];
        if (!has_previous || previous_term != reader->term ||
            previous_document != reader->document_id) {
            const size_t length = reader->term.size();
            if (length > UINT32_MAX) {
                fail("слишком длинный термин при слиянии");
            }
            const uint32_t stored_length = static_cast<uint32_t>(length);
            if (std::fwrite(&stored_length, sizeof(stored_length), 1, output) != 1 ||
                std::fwrite(reader->term.data(), 1, length, output) != length ||
                std::fwrite(&reader->document_id, sizeof(reader->document_id), 1,
                            output) != 1 ||
                std::fwrite(&reader->term_frequency,
                            sizeof(reader->term_frequency), 1, output) != 1) {
                fail("ошибка записи промежуточного блока");
            }
            previous_term = reader->term;
            previous_document = reader->document_id;
            has_previous = true;
        }
        if (!block_reader_next(reader)) {
            fail("повреждён блок при промежуточном слиянии");
        }
        if (reader->active) {
            heap_sift_down(&heap, 0);
        } else {
            heap.items[0] = heap.items.back();
            heap.items.pop_back();
            if (!heap.items.empty()) {
                heap_sift_down(&heap, 0);
            }
        }
    }
    for (BlockReader &reader : readers) {
        std::fclose(reader.file);
    }
    if (std::fclose(output) != 0) {
        fail("ошибка закрытия промежуточного блока");
    }
}

}  // namespace

bool posting_buffer_add(PostingBuffer *buffer, const char *term,
                        size_t term_size, uint32_t document_id,
                        uint32_t term_frequency) {
    buffer->push_back(
        {std::string(term, term_size), document_id, term_frequency});
    return true;
}

void flush_block(PostingBuffer *buffer, BlockList *blocks,
                 const char *temporary_directory) {
    if (buffer->empty()) {
        return;
    }
    std::sort(buffer->begin(), buffer->end(), posting_pair_before);
    buffer->erase(std::unique(buffer->begin(), buffer->end(),
                              posting_pairs_equal),
                  buffer->end());
    char name[64];
    std::snprintf(name, sizeof(name), "block_%06zu.bin", blocks->size());
    char path[PATH_SIZE];
    join_path(path, sizeof(path), temporary_directory, name);
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr) {
        fail("не удалось создать временный блок");
    }
    for (const PostingPair &pair : *buffer) {
        const size_t length = pair.term.size();
        if (length > UINT32_MAX) {
            fail("слишком длинный термин");
        }
        const uint32_t stored_length = static_cast<uint32_t>(length);
        if (std::fwrite(&stored_length, sizeof(stored_length), 1, file) != 1 ||
            std::fwrite(pair.term.data(), 1, length, file) != length ||
            std::fwrite(&pair.document_id, sizeof(pair.document_id), 1, file) != 1 ||
            std::fwrite(&pair.term_frequency, sizeof(pair.term_frequency), 1,
                        file) != 1) {
            fail("ошибка записи временного блока");
        }
    }
    if (std::fclose(file) != 0) {
        fail("ошибка закрытия временного блока");
    }
    blocks->emplace_back(path);
    buffer->clear();
    std::fprintf(stderr, "Индекс: записан блок %zu\n", blocks->size());
}

bool block_reader_next(BlockReader *reader) {
    reader->term.clear();
    uint32_t length = 0;
    if (std::fread(&length, sizeof(length), 1, reader->file) != 1) {
        reader->active = false;
        return std::feof(reader->file) != 0;
    }
    reader->term.resize(length);
    if (std::fread(reader->term.data(), 1, length, reader->file) != length ||
        std::fread(&reader->document_id, sizeof(reader->document_id), 1,
                   reader->file) != 1 ||
        std::fread(&reader->term_frequency, sizeof(reader->term_frequency), 1,
                   reader->file) != 1) {
        reader->term.clear();
        reader->active = false;
        return false;
    }
    reader->active = true;
    return true;
}

void heap_sift_down(ReaderHeap *heap, size_t position) {
    while (true) {
        const size_t left = position * 2 + 1;
        const size_t right = left + 1;
        size_t smallest = position;
        if (left < heap->items.size() &&
            reader_before(heap, heap->items[left], heap->items[smallest])) {
            smallest = left;
        }
        if (right < heap->items.size() &&
            reader_before(heap, heap->items[right], heap->items[smallest])) {
            smallest = right;
        }
        if (smallest == position) {
            return;
        }
        std::swap(heap->items[position], heap->items[smallest]);
        position = smallest;
    }
}

void reduce_blocks(BlockList *blocks, const char *temporary_directory) {
    size_t pass = 0;
    while (blocks->size() > MERGE_FAN_IN) {
        BlockList reduced{};
        for (size_t begin = 0; begin < blocks->size(); begin += MERGE_FAN_IN) {
            const size_t remaining = blocks->size() - begin;
            const size_t count = remaining < MERGE_FAN_IN
                                     ? remaining
                                     : MERGE_FAN_IN;
            char name[96];
            std::snprintf(name, sizeof(name), "merge_%03zu_%06zu.bin", pass,
                          reduced.size());
            char path[PATH_SIZE];
            join_path(path, sizeof(path), temporary_directory, name);
            merge_raw_group(*blocks, begin, count, path);
            reduced.emplace_back(path);
            for (size_t index = begin; index < begin + count; ++index) {
                unlink((*blocks)[index].c_str());
            }
        }
        *blocks = std::move(reduced);
        ++pass;
        std::fprintf(stderr,
                     "Индекс: промежуточное слияние, осталось блоков %zu\n",
                     blocks->size());
    }
}
