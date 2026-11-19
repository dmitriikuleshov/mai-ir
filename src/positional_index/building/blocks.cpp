// Сортирует координаты ограниченными блоками и сливает их на диске.
#include "positional_index/building/blocks.hpp"

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

bool occurrence_before(const PositionOccurrence &left,
                       const PositionOccurrence &right) {
    return left.term < right.term ||
           (left.term == right.term &&
            (left.document_id < right.document_id ||
             (left.document_id == right.document_id &&
              left.position < right.position)));
}

bool occurrences_equal(const PositionOccurrence &left,
                       const PositionOccurrence &right) {
    return left.term == right.term &&
           left.document_id == right.document_id &&
           left.position == right.position;
}

bool reader_before(const PositionReaderHeap *heap, size_t first,
                   size_t second) {
    const PositionBlockReader &left = (*heap->readers)[first];
    const PositionBlockReader &right = (*heap->readers)[second];
    return left.term < right.term ||
           (left.term == right.term &&
            (left.document_id < right.document_id ||
             (left.document_id == right.document_id &&
              (left.position < right.position ||
               (left.position == right.position && first < second)))));
}

bool write_occurrence(FILE *file, const PositionBlockReader &reader) {
    if (reader.term.size() > UINT32_MAX) return false;
    const uint32_t length = static_cast<uint32_t>(reader.term.size());
    return std::fwrite(&length, sizeof(length), 1, file) == 1 &&
           std::fwrite(reader.term.data(), 1, length, file) == length &&
           std::fwrite(&reader.document_id, sizeof(reader.document_id), 1,
                       file) == 1 &&
           std::fwrite(&reader.position, sizeof(reader.position), 1, file) == 1;
}

void merge_raw_group(const PositionBlockList &paths, size_t begin,
                     size_t count, const char *output_path) {
    FILE *output = std::fopen(output_path, "wb");
    if (output == nullptr) fail("не удалось создать промежуточный блок координат");

    std::vector<PositionBlockReader> readers(count);
    PositionReaderHeap heap{{}, &readers};
    heap.items.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        readers[index].file = std::fopen(paths[begin + index].c_str(), "rb");
        if (readers[index].file == nullptr ||
            !position_block_reader_next(&readers[index])) {
            fail("не удалось прочитать блок координат");
        }
        if (readers[index].active) heap.items.push_back(index);
    }
    for (size_t position = heap.items.size() / 2; position > 0; --position) {
        position_heap_sift_down(&heap, position - 1);
    }

    PositionOccurrence previous{};
    bool has_previous = false;
    while (!heap.items.empty()) {
        const size_t reader_index = heap.items[0];
        PositionBlockReader *reader = &readers[reader_index];
        const PositionOccurrence current{
            reader->term, reader->document_id, reader->position};
        if (!has_previous || !occurrences_equal(previous, current)) {
            if (!write_occurrence(output, *reader)) {
                fail("ошибка записи промежуточного блока координат");
            }
            previous = current;
            has_previous = true;
        }
        if (!position_block_reader_next(reader)) {
            fail("повреждён блок координат");
        }
        if (reader->active) {
            position_heap_sift_down(&heap, 0);
        } else {
            heap.items[0] = heap.items.back();
            heap.items.pop_back();
            if (!heap.items.empty()) position_heap_sift_down(&heap, 0);
        }
    }
    for (PositionBlockReader &reader : readers) std::fclose(reader.file);
    if (std::fclose(output) != 0) fail("ошибка закрытия блока координат");
}

}  // namespace

void position_buffer_add(PositionBuffer *buffer, const std::string &term,
                         uint32_t document_id, uint32_t position) {
    buffer->push_back({term, document_id, position});
}

void flush_position_block(PositionBuffer *buffer, PositionBlockList *blocks,
                          const char *temporary_directory) {
    if (buffer->empty()) return;
    std::sort(buffer->begin(), buffer->end(), occurrence_before);
    buffer->erase(std::unique(buffer->begin(), buffer->end(), occurrences_equal),
                  buffer->end());

    char name[64];
    std::snprintf(name, sizeof(name), "positions_%06zu.bin", blocks->size());
    char path[PATH_SIZE];
    join_path(path, sizeof(path), temporary_directory, name);
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr) fail("не удалось создать блок координат");
    for (const PositionOccurrence &occurrence : *buffer) {
        PositionBlockReader value{nullptr, occurrence.term,
                                  occurrence.document_id,
                                  occurrence.position, true};
        if (!write_occurrence(file, value)) {
            fail("ошибка записи блока координат");
        }
    }
    if (std::fclose(file) != 0) fail("ошибка закрытия блока координат");
    blocks->emplace_back(path);
    buffer->clear();
    std::fprintf(stderr, "Координатный индекс: записан блок %zu\n",
                 blocks->size());
}

bool position_block_reader_next(PositionBlockReader *reader) {
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
        std::fread(&reader->position, sizeof(reader->position), 1,
                   reader->file) != 1) {
        reader->active = false;
        reader->term.clear();
        return false;
    }
    reader->active = true;
    return true;
}

void position_heap_sift_down(PositionReaderHeap *heap, size_t position) {
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
        if (smallest == position) return;
        std::swap(heap->items[position], heap->items[smallest]);
        position = smallest;
    }
}

void reduce_position_blocks(PositionBlockList *blocks,
                            const char *temporary_directory) {
    size_t pass = 0;
    while (blocks->size() > MERGE_FAN_IN) {
        PositionBlockList reduced;
        for (size_t begin = 0; begin < blocks->size(); begin += MERGE_FAN_IN) {
            const size_t remaining = blocks->size() - begin;
            const size_t count = remaining < MERGE_FAN_IN
                                     ? remaining
                                     : MERGE_FAN_IN;
            char name[96];
            std::snprintf(name, sizeof(name), "positions_merge_%03zu_%06zu.bin",
                          pass, reduced.size());
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
                     "Координатный индекс: промежуточное слияние, блоков %zu\n",
                     blocks->size());
    }
}
