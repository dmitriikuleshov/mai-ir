// Описывает бинарный формат координатного индекса для цитатного поиска.
#ifndef MAI_IR_POSITIONAL_INDEX_FORMAT_HPP
#define MAI_IR_POSITIONAL_INDEX_FORMAT_HPP

#include <cstdint>

namespace positional_index_format {

constexpr uint32_t VERSION = 1;
constexpr char MAGIC[8] = {'M', 'A', 'I', 'I', 'R', 'P', 'O', 'S'};

#pragma pack(push, 1)
// Заголовок связывает координаты с конкретным набором внутренних docID.
struct Header {
    char magic[8];
    uint32_t version;
    uint32_t document_count;
    uint64_t term_count;
    uint64_t posting_count;
    uint64_t position_count;
};

// Запись указывает на строку терма и его координатный posting list.
struct LexiconEntry {
    uint64_t term_offset;
    uint64_t postings_offset;
    uint32_t term_length;
    uint32_t postings_count;
    uint64_t position_count;
};
#pragma pack(pop)

static_assert(sizeof(Header) == 40);
static_assert(sizeof(LexiconEntry) == 32);

}  // namespace positional_index_format

#endif
