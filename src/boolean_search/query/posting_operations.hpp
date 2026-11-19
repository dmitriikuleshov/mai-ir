// Объявляет булевы операции над отсортированными списками docID.
#ifndef MAI_IR_BOOLEAN_SEARCH_POSTING_OPERATIONS_HPP
#define MAI_IR_BOOLEAN_SEARCH_POSTING_OPERATIONS_HPP

#include "boolean_search/index/types.hpp"

// Счётчики позволяют доказать ускорение без зависимости от нагрузки компьютера.
struct IntersectionStats {
    uint64_t comparisons = 0;
    uint64_t skip_jumps = 0;
    uint64_t skipped_postings = 0;
};

// Строит временные sqrt-указатели для результата составного выражения.
BooleanPostingList make_posting_list(BooleanResult documents,
                                     size_t minimum_postings);

// Вычисляет AND линейно или с сохранёнными указателями прыжков.
BooleanResult intersect_postings(const BooleanPostingList &left,
                                 const BooleanPostingList &right,
                                 bool use_skips,
                                 IntersectionStats *stats = nullptr);

// Линейная перегрузка сохраняет простой интерфейс для базовых проверок.
BooleanResult intersect_postings(const BooleanResult &left,
                                 const BooleanResult &right);

// Вычисляет объединение posting lists, соответствующее оператору OR.
BooleanResult unite_postings(const BooleanResult &left,
                             const BooleanResult &right);

// Строит дополнение списка относительно всех документов индекса для NOT.
BooleanResult complement_postings(const BooleanResult &input,
                                  uint32_t document_count);

#endif
