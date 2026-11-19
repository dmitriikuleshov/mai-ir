// Объявляет консольный интерфейс цитатного поиска.
#ifndef MAI_IR_PHRASE_SEARCH_CONSOLE_HPP
#define MAI_IR_PHRASE_SEARCH_CONSOLE_HPP

#include <mongoc/mongoc.h>

#include "boolean_search/app/config.hpp"
#include "linguistics/language/lemmatizer.hpp"
#include "phrase_search/index/types.hpp"

void run_phrase_search_console(const BooleanSearchConfig &config,
                               PositionalIndex *index,
                               const LemmaDictionary *dictionary,
                               mongoc_collection_t *collection);

#endif
