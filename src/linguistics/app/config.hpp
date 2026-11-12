#ifndef MAI_IR_LINGUISTICS_CONFIG_HPP
#define MAI_IR_LINGUISTICS_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>

struct Config {
    std::string uri;
    std::string database;
    std::string documents;
    std::string processed;
    std::string document_terms;
    std::string token_frequencies;
    std::string lemma_frequencies;
    std::string state;
    std::string lemma_dictionary;
    std::string lemma_vocabulary;
    std::string lemma_overrides;
    std::string output_directory;
    size_t batch_size = 0;
    size_t minimum_words = 0;
    size_t maximum_token_length = 0;
    uint32_t parser_version = 0;
    uint64_t max_documents = 0;
    size_t progress_every = 0;
    size_t zipf_max_terms = 0;
    size_t zipf_fit_terms = 0;
    size_t top_terms = 0;
    uint64_t lock_lease_seconds = 0;
};

Config load_config(const char *path);

#endif
