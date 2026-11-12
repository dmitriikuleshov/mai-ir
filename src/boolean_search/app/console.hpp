// Объявляет консольный цикл ввода запросов и показа найденных документов.
#ifndef MAI_IR_BOOLEAN_SEARCH_CONSOLE_HPP
#define MAI_IR_BOOLEAN_SEARCH_CONSOLE_HPP

#include <mongoc/mongoc.h>

#include "boolean_search/app/config.hpp"
#include "boolean_search/query/query_parser.hpp"

// Запускает интерактивную или конвейерную обработку булевых запросов.
void run_boolean_search_console(const BooleanSearchConfig &config,
                                BooleanIndex *index,
                                const LemmaDictionary *dictionary,
                                mongoc_collection_t *collection);

#endif
