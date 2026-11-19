// Открывает бинарный индекс и извлекает термы, postings и ObjectId документов.
#include "boolean_search/index/index_reader.hpp"

#include "boolean_index/compression/variable_byte.hpp"

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

using boolean_index_format::DocumentHeader;
using boolean_index_format::INDEX_MAGIC;
using boolean_index_format::LexiconEntry;
using boolean_index_format::SkipEntry;
using boolean_index_format::SkipTermEntry;

namespace {

// Возвращает типизированный FILE* из непрозрачного поля публичной структуры.
FILE *as_file(void *pointer) {
    return static_cast<FILE *>(pointer);
}

// Записывает диагностическое сообщение в переданный вызывающим кодом буфер.
void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

// Формирует полный путь к одному файлу каталога индекса.
std::string make_path(const char *directory, const char *name) {
    return std::string(directory) + "/" + name;
}

// Читает ровно заданное число байтов или сообщает о неполном файле.
bool read_exact(FILE *file, void *target, size_t size) {
    return std::fread(target, 1, size, file) == size;
}

// Безопасно переходит к 64-битному байтовому смещению в файле.
bool seek_file(FILE *file, uint64_t offset) {
    return offset <= static_cast<uint64_t>(INT64_MAX) &&
           fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
}

// Читает текст терма по записи лексикона и сравнивает его с искомой строкой.
bool compare_term(BooleanIndex *index, const LexiconEntry &entry,
                  const char *term, std::string *buffer, int *comparison) {
    buffer->resize(entry.term_length);
    FILE *terms = as_file(index->terms_file);
    if (!seek_file(terms, entry.term_offset) ||
        !read_exact(terms, buffer->data(), entry.term_length)) {
        return false;
    }
    *comparison = buffer->compare(term);
    return true;
}

// Возвращает запись лексикона и её порядковый номер для поиска в skips.bin.
bool find_term_with_index(BooleanIndex *index, const char *term,
                          LexiconEntry *result, uint64_t *term_index) {
    uint64_t left = 0;
    uint64_t right = index->header.term_count;
    std::string buffer;
    FILE *lexicon = as_file(index->lexicon_file);
    while (left < right) {
        const uint64_t middle = left + (right - left) / 2;
        const uint64_t offset = sizeof(boolean_index_format::IndexHeader) +
                                middle * sizeof(LexiconEntry);
        LexiconEntry entry{};
        if (!seek_file(lexicon, offset) ||
            !read_exact(lexicon, &entry, sizeof(entry))) {
            return false;
        }
        int comparison = 0;
        if (!compare_term(index, entry, term, &buffer, &comparison)) {
            return false;
        }
        if (comparison == 0) {
            *result = entry;
            if (term_index != nullptr) *term_index = middle;
            return true;
        }
        if (comparison < 0) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return false;
}

// Ищет разреженную запись терма и читает соответствующие ей указатели.
bool load_skip_pointers(BooleanIndex *index, uint64_t term_index,
                        const BooleanResult &documents,
                        std::vector<SkipPointer> *result) {
    FILE *file = as_file(index->skips_file);
    uint64_t left = 0;
    uint64_t right = index->skip_header.skip_term_count;
    while (left < right) {
        const uint64_t middle = left + (right - left) / 2;
        const uint64_t offset = sizeof(boolean_index_format::SkipHeader) +
                                middle * sizeof(SkipTermEntry);
        SkipTermEntry term{};
        if (!seek_file(file, offset) ||
            !read_exact(file, &term, sizeof(term))) {
            return false;
        }
        if (term.term_index < term_index) {
            left = middle + 1;
            continue;
        }
        if (term.term_index > term_index) {
            right = middle;
            continue;
        }

        const uint64_t entries_begin =
            sizeof(boolean_index_format::SkipHeader) +
            index->skip_header.skip_term_count * sizeof(SkipTermEntry);
        if (term.first_skip > index->skip_header.skip_entry_count ||
            term.skip_count >
                index->skip_header.skip_entry_count - term.first_skip) {
            return false;
        }
        const uint64_t entries_offset =
            entries_begin + term.first_skip * sizeof(SkipEntry);
        if (!seek_file(file, entries_offset)) return false;
        result->reserve(term.skip_count);
        for (uint32_t position = 0; position < term.skip_count; ++position) {
            SkipEntry entry{};
            if (!read_exact(file, &entry, sizeof(entry)) ||
                entry.posting_index >= documents.size() ||
                documents[entry.posting_index] != entry.document_id ||
                (!result->empty() &&
                 (result->back().posting_index >= entry.posting_index ||
                  result->back().document_id >= entry.document_id))) {
                result->clear();
                return false;
            }
            result->push_back({entry.posting_index, entry.document_id});
        }
        return true;
    }
    return true;
}

// Последовательно декодирует известную запись лексикона без повторного поиска терма.
WeightedPostingList load_weighted_entry(BooleanIndex *index,
                                        const LexiconEntry &entry) {
    WeightedPostingList result;
    result.reserve(entry.postings_count);
    FILE *postings = as_file(index->postings_file);
    if (!seek_file(postings, entry.postings_offset)) return {};
    uint32_t document_id = 0;
    for (uint32_t position = 0; position < entry.postings_count; ++position) {
        uint32_t document_gap = 0;
        uint32_t term_frequency = 0;
        if (!variable_byte_read(postings, &document_gap) ||
            !variable_byte_read(postings, &term_frequency) ||
            term_frequency == 0 || document_gap > UINT32_MAX - document_id ||
            (position != 0 && document_gap == 0)) {
            return {};
        }
        document_id += document_gap;
        result.push_back({document_id, term_frequency});
    }
    return result;
}

}  // namespace

// Находит терм двоичным поиском по отсортированным записям lexicon.bin.
bool boolean_index_find_term(BooleanIndex *index, const char *term,
                             LexiconEntry *result) {
    return find_term_with_index(index, term, result, nullptr);
}

// Открывает четыре файла индекса и проверяет их magic, версию и число документов.
bool boolean_index_open(BooleanIndex *index, const char *directory,
                        char *error, size_t error_size) {
    *index = {};
    const char *names[] = {"lexicon.bin", "terms.bin", "postings.bin",
                           "documents.bin", "skips.bin"};
    void **targets[] = {&index->lexicon_file, &index->terms_file,
                        &index->postings_file, &index->documents_file,
                        &index->skips_file};
    for (size_t item = 0; item < 5; ++item) {
        const std::string path = make_path(directory, names[item]);
        FILE *file = std::fopen(path.c_str(), "rb");
        if (file == nullptr) {
            set_error(error, error_size, "не удалось открыть файлы индекса");
            boolean_index_close(index);
            return false;
        }
        *targets[item] = file;
    }
    if (!read_exact(as_file(index->lexicon_file), &index->header,
                    sizeof(index->header)) ||
        std::memcmp(index->header.magic, INDEX_MAGIC, sizeof(INDEX_MAGIC)) != 0 ||
        index->header.version != boolean_index_format::VERSION) {
        set_error(error, error_size, "неверный формат или версия индекса");
        boolean_index_close(index);
        return false;
    }
    DocumentHeader documents{};
    if (!read_exact(as_file(index->documents_file), &documents,
                    sizeof(documents)) ||
        std::memcmp(documents.magic, boolean_index_format::DOCUMENT_MAGIC,
                    sizeof(documents.magic)) != 0 ||
        documents.version != boolean_index_format::VERSION ||
        documents.document_count != index->header.document_count) {
        set_error(error, error_size,
                  "таблица документов не соответствует индексу");
        boolean_index_close(index);
        return false;
    }
    if (!read_exact(as_file(index->skips_file), &index->skip_header,
                    sizeof(index->skip_header)) ||
        std::memcmp(index->skip_header.magic,
                    boolean_index_format::SKIP_MAGIC,
                    sizeof(boolean_index_format::SKIP_MAGIC)) != 0 ||
        index->skip_header.version != boolean_index_format::VERSION ||
        index->skip_header.document_count != index->header.document_count ||
        index->skip_header.term_count != index->header.term_count) {
        set_error(error, error_size,
                  "таблица прыжков не соответствует индексу");
        boolean_index_close(index);
        return false;
    }
    return true;
}

// Закрывает все файлы индекса и обнуляет его состояние.
void boolean_index_close(BooleanIndex *index) {
    void **files[] = {&index->lexicon_file, &index->terms_file,
                      &index->postings_file, &index->documents_file,
                      &index->skips_file};
    for (void **file : files) {
        if (*file != nullptr) {
            std::fclose(as_file(*file));
            *file = nullptr;
        }
    }
    index->header = {};
    index->skip_header = {};
}

// Читает ObjectId MongoDB по позиции, соответствующей внутреннему docID.
bool boolean_index_document_oid(BooleanIndex *index, uint32_t document_id,
                                uint8_t oid[12]) {
    if (document_id >= index->header.document_count) {
        return false;
    }
    FILE *documents = as_file(index->documents_file);
    const uint64_t offset = sizeof(DocumentHeader) +
                            static_cast<uint64_t>(document_id) * 12;
    return seek_file(documents, offset) && read_exact(documents, oid, 12);
}

// Загружает posting list вместе с частотой терма в каждом документе.
WeightedPostingList load_weighted_postings(BooleanIndex *index,
                                           const char *term) {
    LexiconEntry entry{};
    if (!boolean_index_find_term(index, term, &entry) ||
        entry.postings_count == 0) {
        return {};
    }
    return load_weighted_entry(index, entry);
}

// Загружает только docID posting list, отбрасывая ненужные булеву поиску частоты.
BooleanResult load_postings(BooleanIndex *index, const char *term) {
    return load_boolean_postings(index, term).documents;
}

// Загружает posting list и проверенные указатели прыжков этого терма.
BooleanPostingList load_boolean_postings(BooleanIndex *index,
                                         const char *term) {
    LexiconEntry entry{};
    uint64_t term_index = 0;
    if (!find_term_with_index(index, term, &entry, &term_index) ||
        entry.postings_count == 0) {
        return {};
    }
    BooleanPostingList result;
    const WeightedPostingList weighted = load_weighted_entry(index, entry);
    if (weighted.size() != entry.postings_count) return {};
    result.documents.reserve(weighted.size());
    for (const WeightedPosting &posting : weighted) {
        result.documents.push_back(posting.document_id);
    }
    if (!load_skip_pointers(index, term_index, result.documents,
                            &result.skips)) {
        return {};
    }
    return result;
}
