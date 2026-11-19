// Объявляет финальное слияние блоков в координатный индекс.
#ifndef MAI_IR_POSITIONAL_INDEX_WRITER_HPP
#define MAI_IR_POSITIONAL_INDEX_WRITER_HPP

#include <cstdint>

#include "positional_index/building/blocks.hpp"

void merge_position_blocks(const PositionBlockList *blocks,
                           const char *output_directory,
                           uint32_t document_count);

#endif
