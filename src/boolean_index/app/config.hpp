#ifndef MAI_IR_BOOLEAN_INDEX_CONFIG_HPP
#define MAI_IR_BOOLEAN_INDEX_CONFIG_HPP

#include <cstddef>
#include <string>

struct BooleanIndexConfig {
    std::string uri;
    std::string database;
    std::string terms_collection;
    std::string output_directory;
    std::string term_field;
    size_t block_postings;
    size_t progress_every_documents;
};

BooleanIndexConfig load_boolean_index_config(const char *path);

#endif
