#ifndef MAI_IR_BOOLEAN_SEARCH_CONFIG_HPP
#define MAI_IR_BOOLEAN_SEARCH_CONFIG_HPP

#include <cstddef>
#include <string>

struct BooleanSearchConfig {
    std::string uri;
    std::string database;
    std::string processed_collection;
    std::string index_directory;
    std::string lemma_dictionary;
    std::string lemma_vocabulary;
    std::string lemma_overrides;
    size_t maximum_token_length;
    size_t result_limit;
    size_t tfidf_result_limit;
};

BooleanSearchConfig load_boolean_search_config(const char *path);

#endif
