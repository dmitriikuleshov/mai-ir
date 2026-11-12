// Объявляет финальное слияние блоков и завершение вспомогательных таблиц индекса.
#ifndef MAI_IR_BOOLEAN_INDEX_INDEX_WRITER_HPP
#define MAI_IR_BOOLEAN_INDEX_INDEX_WRITER_HPP

#include <cstdint>
#include <cstdio>

#include "boolean_index/building/blocks.hpp"

// Сливает отсортированные блоки в лексикон, словарь термов и сжатые postings.
void merge_blocks(const BlockList *blocks, const char *output_directory,
                  uint32_t document_count);

// Завершает временную таблицу соответствия внутренних docID и ObjectId.
void finalize_documents(FILE *documents, const char *temporary_path,
                        const char *output_directory, uint32_t count);

// Завершает временную таблицу L2-норм документов.
void finalize_document_norms(FILE *norms, const char *temporary_path,
                             const char *output_directory, uint32_t count);

#endif
