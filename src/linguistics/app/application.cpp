#include "linguistics/app/application.hpp"

#include "linguistics/processing/document_processor.hpp"
#include "linguistics/reporting/report.hpp"
#include "linguistics/storage/mongo_storage.hpp"

#include <libxml/parser.h>
#include <mongoc/mongoc.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {

double monotonic_seconds() {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(elapsed).count();
}

void log_stop() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local);
    std::printf(
        "%s | INFO     | Обработка остановлена по сигналу; "
        "данные сохранены согласованно\n",
        timestamp);
}

}  // namespace

int run_linguistics(const Config &config) {
    const double program_started = monotonic_seconds();
    std::signal(SIGINT, request_processing_stop);
    std::signal(SIGTERM, request_processing_stop);

    mongoc_init();
    xmlInitParser();
    mongoc_client_t *client = mongoc_client_new(config.uri.c_str());
    if (client == nullptr) {
        std::fprintf(stderr, "linguistics: некорректный URI MongoDB\n");
        return EXIT_FAILURE;
    }
    mongoc_client_set_appname(client, "mai-ir-linguistics-cpp");
    ping_database(client, config.database.c_str());
    MongoCollections collections = open_collections(client, config);
    LemmaDictionary *dictionary = lemma_dictionary_load(
        config.lemma_dictionary.c_str(), config.lemma_vocabulary.c_str(),
        config.lemma_overrides.c_str());

    ProcessingLock lock = acquire_processing_lock(
        collections.state, config.lock_lease_seconds);
    prepare_frequencies(collections, config.parser_version);
    create_indexes(collections);
    const Counters counters =
        process_documents(collections, &lock, config, dictionary);
    set_frequency_state(collections.state, false, config.parser_version);
    write_report(config, counters, collections, dictionary,
                 monotonic_seconds() - program_started);
    release_processing_lock(collections.state, &lock);

    lemma_dictionary_destroy(dictionary);
    close_collections(&collections);
    mongoc_client_destroy(client);
    xmlCleanupParser();
    mongoc_cleanup();

    if (processing_stop_requested()) {
        log_stop();
        return 130;
    }
    return EXIT_SUCCESS;
}
