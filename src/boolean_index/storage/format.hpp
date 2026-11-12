// Описывает стабильный бинарный формат словаря, postings и таблиц документов.
#ifndef MAI_IR_BOOLEAN_INDEX_FORMAT_HPP
#define MAI_IR_BOOLEAN_INDEX_FORMAT_HPP

#include <cstdint>

namespace boolean_index_format {

// Версия и magic-последовательности не позволяют прочитать несовместимые файлы.
constexpr uint32_t VERSION = 3;
constexpr char INDEX_MAGIC[8] = {'M', 'A', 'I', 'I', 'R', 'I', 'D', 'X'};
constexpr char DOCUMENT_MAGIC[8] = {'M', 'A', 'I', 'I', 'R', 'D', 'O', 'C'};
constexpr char DOCUMENT_NORM_MAGIC[8] = {'M', 'A', 'I', 'I', 'R', 'N', 'R', 'M'};

// Упаковка без выравнивающих байтов фиксирует точный размер структур на диске.
#pragma pack(push, 1)
// Общий заголовок хранит размеры корпуса и итогового инвертированного индекса.
struct IndexHeader {
    char magic[8];
    uint32_t version;
    uint32_t document_count;
    uint64_t term_count;
    uint64_t posting_count;
};

// Запись лексикона связывает терм с байтовыми смещениями в terms и postings.
struct LexiconEntry {
    uint64_t term_offset;
    uint64_t postings_offset;
    uint32_t term_length;
    uint32_t postings_count;
};

// Заголовок таблицы соответствия последовательного docID и ObjectId MongoDB.
struct DocumentHeader {
    char magic[8];
    uint32_t version;
    uint32_t document_count;
};

// Заголовок таблицы L2-норм документов, используемых ранжированием TF-IDF.
struct DocumentNormHeader {
    char magic[8];
    uint32_t version;
    uint32_t document_count;
};
#pragma pack(pop)

// Проверки не позволяют незаметно изменить дисковый формат из-за структуры C++.
static_assert(sizeof(IndexHeader) == 32);
static_assert(sizeof(LexiconEntry) == 24);
static_assert(sizeof(DocumentHeader) == 16);
static_assert(sizeof(DocumentNormHeader) == 16);

}  // namespace boolean_index_format

#endif
