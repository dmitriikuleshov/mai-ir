#ifndef MAI_IR_TFIDF_SEARCH_CONSOLE_HPP
#define MAI_IR_TFIDF_SEARCH_CONSOLE_HPP

#include <mongoc/mongoc.h>

#include "boolean_search/app/config.hpp"
#include "linguistics/language/lemmatizer.hpp"
#include "tfidf_search/index/ranking_index.hpp"

void run_tfidf_search_console(const BooleanSearchConfig &config,
                              TfidfIndex *index,
                              const LemmaDictionary *dictionary,
                              mongoc_collection_t *collection);

#endif
