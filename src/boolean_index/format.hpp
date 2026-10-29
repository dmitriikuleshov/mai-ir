#ifndef MAI_IR_BOOLEAN_INDEX_FORMAT_HPP
#define MAI_IR_BOOLEAN_INDEX_FORMAT_HPP

#include <cstdint>

namespace boolean_index_format {

constexpr uint32_t VERSION = 1;
constexpr char INDEX_MAGIC[8] = {'M', 'A', 'I', 'I', 'R', 'I', 'D', 'X'};
constexpr char DOCUMENT_MAGIC[8] = {'M', 'A', 'I', 'I', 'R', 'D', 'O', 'C'};

#pragma pack(push, 1)
struct IndexHeader {
    char magic[8];
    uint32_t version;
    uint32_t document_count;
    uint64_t term_count;
    uint64_t posting_count;
};

struct LexiconEntry {
    uint64_t term_offset;
    uint64_t postings_offset;
    uint32_t term_length;
    uint32_t postings_count;
};

struct DocumentHeader {
    char magic[8];
    uint32_t version;
    uint32_t document_count;
};
#pragma pack(pop)

static_assert(sizeof(IndexHeader) == 32);
static_assert(sizeof(LexiconEntry) == 24);
static_assert(sizeof(DocumentHeader) == 16);

}  // namespace boolean_index_format

#endif
