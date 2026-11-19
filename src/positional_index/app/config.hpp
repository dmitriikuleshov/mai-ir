// Объявляет настройки построения координатного индекса.
#ifndef MAI_IR_POSITIONAL_INDEX_CONFIG_HPP
#define MAI_IR_POSITIONAL_INDEX_CONFIG_HPP

#include <cstddef>
#include <string>

struct PositionalIndexConfig {
    std::string uri;
    std::string database;
    std::string processed_collection;
    std::string output_directory;
    std::string boolean_index_directory;
    std::string lemma_dictionary;
    std::string lemma_vocabulary;
    std::string lemma_overrides;
    size_t maximum_token_length;
    size_t block_occurrences;
    size_t progress_every_documents;
};

PositionalIndexConfig load_positional_index_config(const char *path);

#endif
