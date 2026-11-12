#ifndef MAI_IR_LINGUISTICS_MONGO_STORAGE_HPP
#define MAI_IR_LINGUISTICS_MONGO_STORAGE_HPP

#include "linguistics/app/config.hpp"

#include <mongoc/mongoc.h>

#include <cstdint>

struct MongoCollections {
    mongoc_collection_t *documents = nullptr;
    mongoc_collection_t *processed = nullptr;
    mongoc_collection_t *terms = nullptr;
    mongoc_collection_t *token_frequencies = nullptr;
    mongoc_collection_t *lemma_frequencies = nullptr;
    mongoc_collection_t *state = nullptr;
};

struct ProcessingLock {
    char owner[25]{};
    uint64_t lease_seconds = 0;
};

void ping_database(mongoc_client_t *client, const char *database);
MongoCollections open_collections(mongoc_client_t *client,
                                  const Config &config);
void close_collections(MongoCollections *collections);

ProcessingLock acquire_processing_lock(mongoc_collection_t *state,
                                       uint64_t lease_seconds);
void update_processing_lock(mongoc_collection_t *state,
                            const ProcessingLock *lock);
void release_processing_lock(mongoc_collection_t *state,
                             const ProcessingLock *lock);

void prepare_frequencies(const MongoCollections &collections,
                         uint32_t parser_version);
void set_frequency_state(mongoc_collection_t *state, bool dirty,
                         uint32_t parser_version);
void create_indexes(const MongoCollections &collections);

#endif
