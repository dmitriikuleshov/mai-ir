// Объявляет извлечение терминов документа и полный процесс построения индекса.
#ifndef MAI_IR_BOOLEAN_INDEX_BUILDER_HPP
#define MAI_IR_BOOLEAN_INDEX_BUILDER_HPP

#include <bson/bson.h>

#include "boolean_index/app/config.hpp"
#include "boolean_index/building/blocks.hpp"

// Добавляет термы одного BSON-документа в буфер и при необходимости возвращает его норму.
bool add_document_terms(const bson_t *document, const char *field,
                        uint32_t document_id, PostingBuffer *buffer,
                        double *document_norm = nullptr);

// Строит дисковый инвертированный индекс по коллекции обработанных документов.
void build_boolean_index(const BooleanIndexConfig &config);

#endif
