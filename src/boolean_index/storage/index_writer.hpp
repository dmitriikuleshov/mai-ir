#ifndef MAI_IR_BOOLEAN_INDEX_INDEX_WRITER_HPP
#define MAI_IR_BOOLEAN_INDEX_INDEX_WRITER_HPP

#include <cstdint>
#include <cstdio>

#include "boolean_index/building/blocks.hpp"

void merge_blocks(const BlockList *blocks, const char *output_directory,
                  uint32_t document_count);
void finalize_documents(FILE *documents, const char *temporary_path,
                        const char *output_directory, uint32_t count);
void finalize_document_norms(FILE *norms, const char *temporary_path,
                             const char *output_directory, uint32_t count);

#endif
