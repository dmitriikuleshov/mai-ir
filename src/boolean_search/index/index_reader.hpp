// Объявляет загрузку бинарного индекса и чтение его основных структур.
#ifndef MAI_IR_BOOLEAN_SEARCH_INDEX_READER_HPP
#define MAI_IR_BOOLEAN_SEARCH_INDEX_READER_HPP

#include <cstddef>
#include <cstdint>

#include "boolean_search/index/types.hpp"

// Открывает и проверяет согласованность файлов индекса.
bool boolean_index_open(BooleanIndex *index, const char *directory,
                        char *error, size_t error_size);

// Закрывает все открытые файлы индекса.
void boolean_index_close(BooleanIndex *index);

// Получает ObjectId исходного документа по внутреннему числовому docID.
bool boolean_index_document_oid(BooleanIndex *index, uint32_t document_id,
                                uint8_t oid[12]);

// Находит фиксированную запись терма в лексиконе.
bool boolean_index_find_term(
    BooleanIndex *index, const char *term,
    boolean_index_format::LexiconEntry *result);

// Загружает упорядоченный список docID для булевых операций.
BooleanResult load_postings(BooleanIndex *index, const char *term);

// Загружает упорядоченный список docID вместе с частотами для ранжирования.
WeightedPostingList load_weighted_postings(BooleanIndex *index,
                                           const char *term);

#endif
