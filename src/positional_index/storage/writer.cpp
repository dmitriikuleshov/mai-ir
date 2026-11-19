// Записывает словарь и сжатые списки координат после внешней сортировки.
#include "positional_index/storage/writer.hpp"

#include "boolean_index/compression/variable_byte.hpp"
#include "boolean_index/storage/files.hpp"
#include "positional_index/storage/format.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "Ошибка: %s\n", message);
    std::exit(EXIT_FAILURE);
}

bool write_lexicon_entry(FILE *file, uint64_t term_offset,
                         uint64_t postings_offset, uint32_t term_length,
                         uint32_t postings_count, uint64_t position_count) {
    const positional_index_format::LexiconEntry entry{
        term_offset, postings_offset, term_length, postings_count,
        position_count};
    return std::fwrite(&entry, sizeof(entry), 1, file) == 1;
}

}  // namespace

void merge_position_blocks(const PositionBlockList *blocks,
                           const char *output_directory,
                           uint32_t document_count) {
    char lexicon_path[PATH_SIZE];
    char terms_path[PATH_SIZE];
    char coordinates_path[PATH_SIZE];
    join_path(lexicon_path, sizeof(lexicon_path), output_directory,
              "coordinate_lexicon.bin.tmp");
    join_path(terms_path, sizeof(terms_path), output_directory,
              "coordinate_terms.bin.tmp");
    join_path(coordinates_path, sizeof(coordinates_path), output_directory,
              "coordinates.bin.tmp");

    FILE *lexicon = std::fopen(lexicon_path, "wb+");
    FILE *terms = std::fopen(terms_path, "wb");
    FILE *coordinates = std::fopen(coordinates_path, "wb");
    if (lexicon == nullptr || terms == nullptr || coordinates == nullptr) {
        fail("не удалось создать файлы координатного индекса");
    }

    positional_index_format::Header header{};
    std::memcpy(header.magic, positional_index_format::MAGIC,
                sizeof(header.magic));
    header.version = positional_index_format::VERSION;
    header.document_count = document_count;
    if (std::fwrite(&header, sizeof(header), 1, lexicon) != 1) {
        fail("не удалось записать заголовок координатного индекса");
    }

    std::vector<PositionBlockReader> readers(blocks->size());
    PositionReaderHeap heap{{}, &readers};
    heap.items.reserve(blocks->size());
    for (size_t index = 0; index < blocks->size(); ++index) {
        readers[index].file = std::fopen((*blocks)[index].c_str(), "rb");
        if (readers[index].file == nullptr ||
            !position_block_reader_next(&readers[index])) {
            fail("не удалось прочитать блок при слиянии координат");
        }
        if (readers[index].active) heap.items.push_back(index);
    }
    for (size_t position = heap.items.size() / 2; position > 0; --position) {
        position_heap_sift_down(&heap, position - 1);
    }

    std::string current_term;
    bool has_term = false;
    uint64_t term_offset = 0;
    uint64_t postings_offset = 0;
    uint32_t term_posting_count = 0;
    uint64_t term_position_count = 0;
    uint32_t last_document = 0;
    bool has_last_document = false;

    uint32_t current_document = 0;
    bool has_document = false;
    std::vector<uint32_t> positions;

    auto finish_document = [&]() {
        if (!has_document) return;
        if (positions.empty() || positions.size() > UINT32_MAX) {
            fail("некорректное число координат терма в документе");
        }
        const uint32_t document_gap =
            has_last_document ? current_document - last_document
                              : current_document;
        if ((has_last_document && document_gap == 0) ||
            !variable_byte_write(coordinates, document_gap) ||
            !variable_byte_write(coordinates,
                                 static_cast<uint32_t>(positions.size()))) {
            fail("ошибка записи координатного posting");
        }
        uint32_t previous_position = 0;
        for (size_t index = 0; index < positions.size(); ++index) {
            const uint32_t gap = index == 0
                                     ? positions[index]
                                     : positions[index] - previous_position;
            if ((index != 0 && gap == 0) ||
                !variable_byte_write(coordinates, gap)) {
                fail("ошибка записи позиции терма");
            }
            previous_position = positions[index];
        }
        last_document = current_document;
        has_last_document = true;
        ++term_posting_count;
        ++header.posting_count;
        term_position_count += positions.size();
        header.position_count += positions.size();
        positions.clear();
        has_document = false;
    };

    auto finish_term = [&]() {
        if (!has_term) return;
        if (current_term.size() > UINT32_MAX || term_posting_count == 0 ||
            !write_lexicon_entry(
                lexicon, term_offset, postings_offset,
                static_cast<uint32_t>(current_term.size()),
                term_posting_count, term_position_count)) {
            fail("ошибка записи координатного лексикона");
        }
        ++header.term_count;
    };

    while (!heap.items.empty()) {
        const size_t reader_index = heap.items[0];
        PositionBlockReader *reader = &readers[reader_index];

        if (!has_term || current_term != reader->term) {
            finish_document();
            finish_term();
            current_term = reader->term;
            has_term = true;
            term_offset = static_cast<uint64_t>(ftello(terms));
            postings_offset = static_cast<uint64_t>(ftello(coordinates));
            term_posting_count = 0;
            term_position_count = 0;
            has_last_document = false;
            if (std::fwrite(current_term.data(), 1, current_term.size(), terms) !=
                current_term.size()) {
                fail("ошибка записи терма координатного индекса");
            }
        }
        if (!has_document || current_document != reader->document_id) {
            finish_document();
            current_document = reader->document_id;
            has_document = true;
        }
        if (!positions.empty() && positions.back() >= reader->position) {
            fail("координаты терма не отсортированы");
        }
        positions.push_back(reader->position);

        if (!position_block_reader_next(reader)) {
            fail("повреждён блок координат при финальном слиянии");
        }
        if (reader->active) {
            position_heap_sift_down(&heap, 0);
        } else {
            heap.items[0] = heap.items.back();
            heap.items.pop_back();
            if (!heap.items.empty()) position_heap_sift_down(&heap, 0);
        }
    }
    finish_document();
    finish_term();

    const off_t compressed_size = ftello(coordinates);
    if (compressed_size < 0 || fseeko(lexicon, 0, SEEK_SET) != 0 ||
        std::fwrite(&header, sizeof(header), 1, lexicon) != 1) {
        fail("не удалось завершить координатный индекс");
    }
    for (PositionBlockReader &reader : readers) {
        if (reader.file != nullptr) std::fclose(reader.file);
    }
    if (std::fclose(lexicon) != 0 || std::fclose(terms) != 0 ||
        std::fclose(coordinates) != 0) {
        fail("ошибка закрытия координатного индекса");
    }

    const char *temporary[] = {lexicon_path, terms_path, coordinates_path};
    const char *names[] = {"coordinate_lexicon.bin", "coordinate_terms.bin",
                           "coordinates.bin"};
    for (size_t index = 0; index < 3; ++index) {
        char final_path[PATH_SIZE];
        join_path(final_path, sizeof(final_path), output_directory,
                  names[index]);
        if (rename(temporary[index], final_path) != 0) {
            fail("не удалось заменить координатный индекс");
        }
    }

    const uint64_t uncompressed =
        header.posting_count * sizeof(uint32_t) +
        header.position_count * sizeof(uint32_t);
    const double saved = uncompressed == 0
                             ? 0.0
                             : 100.0 *
                                   (1.0 - static_cast<double>(compressed_size) /
                                              static_cast<double>(uncompressed));
    std::fprintf(
        stderr,
        "Координатный индекс готов: документов %u, термов %llu, postings %llu, координат %llu\n",
        header.document_count,
        static_cast<unsigned long long>(header.term_count),
        static_cast<unsigned long long>(header.posting_count),
        static_cast<unsigned long long>(header.position_count));
    std::fprintf(stderr,
                 "Сжатие координат: %lld байт вместо %llu байт (%.1f%% экономии)\n",
                 static_cast<long long>(compressed_size),
                 static_cast<unsigned long long>(uncompressed), saved);
}
