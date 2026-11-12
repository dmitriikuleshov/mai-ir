#ifndef MAI_IR_LINGUISTICS_DOCUMENT_PROCESSOR_HPP
#define MAI_IR_LINGUISTICS_DOCUMENT_PROCESSOR_HPP

#include "linguistics/app/config.hpp"
#include "linguistics/language/lemmatizer.hpp"
#include "linguistics/storage/mongo_storage.hpp"

#include <cstdint>

struct Counters {
    uint64_t scanned = 0;
    uint64_t skipped = 0;
    uint64_t processed = 0;
    uint64_t accepted = 0;
    uint64_t rejected = 0;
};

void request_processing_stop(int signal);
bool processing_stop_requested();

Counters process_documents(const MongoCollections &collections,
                           ProcessingLock *lock, const Config &config,
                           const LemmaDictionary *dictionary);

#endif
