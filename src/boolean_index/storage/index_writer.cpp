#include "boolean_index/storage/index_writer.hpp"

#include "boolean_index/compression/variable_byte.hpp"
#include "boolean_index/storage/files.hpp"
#include "boolean_index/storage/format.hpp"

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

bool write_lexicon_entry(FILE *lexicon, uint64_t term_offset,
                         uint64_t postings_offset, uint32_t term_length,
                         uint32_t postings_count) {
    const boolean_index_format::LexiconEntry entry{
        term_offset, postings_offset, term_length, postings_count};
    return std::fwrite(&entry, sizeof(entry), 1, lexicon) == 1;
}

}  // namespace

void merge_blocks(const BlockList *blocks, const char *output_directory,
                  uint32_t document_count) {
    char lexicon_path[PATH_SIZE];
    char terms_path[PATH_SIZE];
    char postings_path[PATH_SIZE];
    join_path(lexicon_path, sizeof(lexicon_path), output_directory,
              "lexicon.bin.tmp");
    join_path(terms_path, sizeof(terms_path), output_directory, "terms.bin.tmp");
    join_path(postings_path, sizeof(postings_path), output_directory,
              "postings.bin.tmp");
    FILE *lexicon = std::fopen(lexicon_path, "wb+");
    FILE *terms = std::fopen(terms_path, "wb");
    FILE *postings = std::fopen(postings_path, "wb");
    if (lexicon == nullptr || terms == nullptr || postings == nullptr) {
        fail("не удалось создать итоговые файлы индекса");
    }
    boolean_index_format::IndexHeader header{};
    std::memcpy(header.magic, boolean_index_format::INDEX_MAGIC,
                sizeof(header.magic));
    header.version = boolean_index_format::VERSION;
    header.document_count = document_count;
    if (std::fwrite(&header, sizeof(header), 1, lexicon) != 1) {
        fail("не удалось записать заголовок индекса");
    }

    std::vector<BlockReader> readers(blocks->size());
    ReaderHeap heap{{}, &readers};
    heap.items.reserve(blocks->size());
    for (size_t index = 0; index < blocks->size(); ++index) {
        readers[index].file = std::fopen((*blocks)[index].c_str(), "rb");
        if (readers[index].file == nullptr || !block_reader_next(&readers[index])) {
            fail("не удалось прочитать временный блок");
        }
        if (readers[index].active) {
            heap.items.push_back(index);
        }
    }
    for (size_t position = heap.items.size() / 2; position > 0; --position) {
        heap_sift_down(&heap, position - 1);
    }

    std::string current_term;
    bool has_current_term = false;
    uint32_t current_length = 0;
    uint64_t current_term_offset = 0;
    uint64_t current_postings_offset = 0;
    uint32_t current_postings_count = 0;
    uint32_t last_document = 0;
    bool has_last_document = false;
    while (!heap.items.empty()) {
        const size_t reader_index = heap.items[0];
        BlockReader *reader = &readers[reader_index];
        const size_t length = reader->term.size();
        if (!has_current_term || current_term != reader->term) {
            if (has_current_term &&
                !write_lexicon_entry(lexicon, current_term_offset,
                                      current_postings_offset, current_length,
                                      current_postings_count)) {
                fail("ошибка записи лексикона");
            }
            if (length > UINT32_MAX) {
                fail("слишком длинный термин при слиянии");
            }
            current_term = reader->term;
            has_current_term = true;
            current_length = static_cast<uint32_t>(length);
            current_term_offset = static_cast<uint64_t>(ftello(terms));
            current_postings_offset = static_cast<uint64_t>(ftello(postings));
            current_postings_count = 0;
            has_last_document = false;
            if (std::fwrite(current_term.data(), 1, length, terms) != length) {
                fail("ошибка записи словаря");
            }
            ++header.term_count;
        }
        if (!has_last_document || last_document != reader->document_id) {
            const uint32_t document_gap = has_last_document
                                              ? reader->document_id - last_document
                                              : reader->document_id;
            if (!variable_byte_write(postings, document_gap) ||
                !variable_byte_write(postings, reader->term_frequency)) {
                fail("ошибка записи сжатого списка документов");
            }
            last_document = reader->document_id;
            has_last_document = true;
            ++current_postings_count;
            ++header.posting_count;
        }

        if (!block_reader_next(reader)) {
            fail("повреждён временный блок");
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
    if (has_current_term &&
        !write_lexicon_entry(lexicon, current_term_offset, current_postings_offset,
                             current_length, current_postings_count)) {
        fail("ошибка записи последнего термина лексикона");
    }
    const off_t compressed_postings_size = ftello(postings);
    if (compressed_postings_size < 0) {
        fail("не удалось определить размер сжатого индекса");
    }
    if (fseeko(lexicon, 0, SEEK_SET) != 0 ||
        std::fwrite(&header, sizeof(header), 1, lexicon) != 1) {
        fail("не удалось обновить заголовок индекса");
    }
    for (BlockReader &reader : readers) {
        std::fclose(reader.file);
    }
    if (std::fclose(lexicon) != 0 || std::fclose(terms) != 0 ||
        std::fclose(postings) != 0) {
        fail("ошибка закрытия итогового индекса");
    }

    const char *temporary[] = {lexicon_path, terms_path, postings_path};
    const char *names[] = {"lexicon.bin", "terms.bin", "postings.bin"};
    for (size_t index = 0; index < 3; ++index) {
        char final_path[PATH_SIZE];
        join_path(final_path, sizeof(final_path), output_directory, names[index]);
        if (rename(temporary[index], final_path) != 0) {
            fail("не удалось заменить старый индекс новым");
        }
    }
    std::fprintf(stderr,
                 "Индекс готов: документов %u, терминов %llu, пар термин-документ %llu\n",
                 header.document_count,
                 static_cast<unsigned long long>(header.term_count),
                 static_cast<unsigned long long>(header.posting_count));
    const uint64_t uncompressed_size =
        header.posting_count * 2U * sizeof(uint32_t);
    const double saved_percent = uncompressed_size == 0
                                     ? 0.0
                                     : 100.0 *
                                           (1.0 - static_cast<double>(
                                                      compressed_postings_size) /
                                                      static_cast<double>(
                                                          uncompressed_size));
    std::fprintf(stderr,
                 "Сжатие postings: %lld байт вместо %llu байт для docID+TF (%.1f%% экономии)\n",
                 static_cast<long long>(compressed_postings_size),
                 static_cast<unsigned long long>(uncompressed_size),
                 saved_percent);
}

void finalize_documents(FILE *documents, const char *temporary_path,
                        const char *output_directory, uint32_t count) {
    boolean_index_format::DocumentHeader header{};
    std::memcpy(header.magic, boolean_index_format::DOCUMENT_MAGIC,
                sizeof(header.magic));
    header.version = boolean_index_format::VERSION;
    header.document_count = count;
    if (fseeko(documents, 0, SEEK_SET) != 0 ||
        std::fwrite(&header, sizeof(header), 1, documents) != 1 ||
        std::fclose(documents) != 0) {
        fail("не удалось завершить таблицу документов");
    }
    char final_path[PATH_SIZE];
    join_path(final_path, sizeof(final_path), output_directory, "documents.bin");
    if (rename(temporary_path, final_path) != 0) {
        fail("не удалось сохранить таблицу документов");
    }
}

void finalize_document_norms(FILE *norms, const char *temporary_path,
                             const char *output_directory, uint32_t count) {
    boolean_index_format::DocumentNormHeader header{};
    std::memcpy(header.magic, boolean_index_format::DOCUMENT_NORM_MAGIC,
                sizeof(header.magic));
    header.version = boolean_index_format::VERSION;
    header.document_count = count;
    if (fseeko(norms, 0, SEEK_SET) != 0 ||
        std::fwrite(&header, sizeof(header), 1, norms) != 1 ||
        std::fclose(norms) != 0) {
        fail("не удалось завершить таблицу норм документов");
    }
    char final_path[PATH_SIZE];
    join_path(final_path, sizeof(final_path), output_directory, "norms.bin");
    if (rename(temporary_path, final_path) != 0) {
        fail("не удалось сохранить таблицу норм документов");
    }
}
