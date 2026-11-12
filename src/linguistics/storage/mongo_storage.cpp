#include "linguistics/storage/mongo_storage.hpp"

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

static void fatal(const char *format, ...) {
    std::fprintf(stderr, "linguistics: ");
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fprintf(stderr, "\n");
    std::exit(EXIT_FAILURE);
}

static void log_message(const char *format, ...) {
    std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_r(&now, &local);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local);
    std::printf("%s | INFO     | ", timestamp);
    va_list arguments;
    va_start(arguments, format);
    std::vprintf(format, arguments);
    va_end(arguments);
    std::printf("\n");
    std::fflush(stdout);
}

static void check_cursor(mongoc_cursor_t *cursor, const char *operation) {
    bson_error_t error = {};
    if (mongoc_cursor_error(cursor, &error)) {
        fatal("%s: %s", operation, error.message);
    }
}

static void check_write(bool ok, const bson_error_t *error, const char *operation) {
    if (!ok) fatal("%s: %s", operation, error->message);
}

static bson_t *json_document(const char *json) {
    bson_error_t error = {};
    bson_t *document = bson_new_from_json(
        reinterpret_cast<const uint8_t *>(json), -1, &error);
    if (document == nullptr) fatal("ошибка BSON JSON: %s", error.message);
    return document;
}

static void append_json_stage(bson_t *pipeline, uint32_t index,
                              const char *json) {
    bson_t *stage = json_document(json);
    char key[32];
    const char *key_pointer = nullptr;
    bson_uint32_to_string(index, &key_pointer, key, sizeof(key));
    bson_append_document(pipeline, key_pointer, -1, stage);
    bson_destroy(stage);
}

static void rebuild_frequencies(mongoc_collection_t *terms,
                                mongoc_collection_t *target,
                                const char *field) {
    char unwind[256];
    char group[1024];
    std::snprintf(unwind, sizeof(unwind), "{\"$unwind\":\"$%s\"}", field);
    std::snprintf(
        group, sizeof(group),
        "{\"$group\":{\"_id\":{\"$arrayElemAt\":[\"$%s\",0]},"
        "\"count\":{\"$sum\":{\"$arrayElemAt\":[\"$%s\",1]}}}}",
        field, field);
    bson_t pipeline;
    bson_init(&pipeline);
    append_json_stage(&pipeline, 0, unwind);
    append_json_stage(&pipeline, 1, group);
    bson_t out;
    bson_init(&out);
    BSON_APPEND_UTF8(&out, "$out", mongoc_collection_get_name(target));
    char key[8];
    const char *key_pointer = nullptr;
    bson_uint32_to_string(2, &key_pointer, key, sizeof(key));
    bson_append_document(&pipeline, key_pointer, -1, &out);
    bson_destroy(&out);
    bson_t opts;
    bson_init(&opts);
    BSON_APPEND_BOOL(&opts, "allowDiskUse", true);
    mongoc_cursor_t *cursor = mongoc_collection_aggregate(
        terms, MONGOC_QUERY_NONE, &pipeline, &opts, nullptr);
    const bson_t *unused = nullptr;
    while (mongoc_cursor_next(cursor, &unused)) {}
    check_cursor(cursor, "восстановление частот");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&opts);
    bson_destroy(&pipeline);
}

static bool collection_has_documents(mongoc_collection_t *collection) {
    bson_error_t error = {};
    const int64_t count = mongoc_collection_estimated_document_count(
        collection, nullptr, nullptr, nullptr, &error);
    if (count < 0) fatal("подсчёт документов: %s", error.message);
    return count > 0;
}

static bool frequency_state_dirty(mongoc_collection_t *state, bool *exists) {
    bson_t *filter = json_document("{\"_id\":\"frequencies\"}");
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        state, filter, nullptr, nullptr);
    const bson_t *document = nullptr;
    bool dirty = false;
    *exists = mongoc_cursor_next(cursor, &document);
    if (*exists) {
        bson_iter_t iterator;
        if (bson_iter_init_find(&iterator, document, "dirty") &&
            BSON_ITER_HOLDS_BOOL(&iterator)) dirty = bson_iter_bool(&iterator);
    }
    check_cursor(cursor, "чтение состояния частот");
    mongoc_cursor_destroy(cursor);
    bson_destroy(filter);
    return dirty;
}

void set_frequency_state(mongoc_collection_t *state, bool dirty,
                                uint32_t parser_version) {
    bson_t *filter = json_document("{\"_id\":\"frequencies\"}");
    bson_t update;
    bson_t values;
    bson_init(&update);
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &values);
    BSON_APPEND_BOOL(&values, "dirty", dirty);
    BSON_APPEND_INT32(&values, "parser_version", parser_version);
    BSON_APPEND_INT64(&values, dirty ? "update_started_at" : "updated_at",
                      static_cast<int64_t>(std::time(nullptr)));
    bson_append_document_end(&update, &values);
    bson_t opts;
    bson_init(&opts);
    BSON_APPEND_BOOL(&opts, "upsert", true);
    bson_error_t error = {};
    bson_t reply;
    bson_init(&reply);
    check_write(mongoc_collection_update_one(state, filter, &update, &opts,
                                              &reply, &error),
                &error, "запись состояния частот");
    bson_destroy(&reply);
    bson_destroy(&opts);
    bson_destroy(&update);
    bson_destroy(filter);
}

void update_processing_lock(mongoc_collection_t *state,
                                   const ProcessingLock *lock) {
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    char filter_json[512];
    std::snprintf(
        filter_json, sizeof(filter_json),
        "{\"_id\":\"processor_lock\",\"$or\":[{\"lock_until\":{\"$lte\":%lld}},"
        "{\"lock_until\":{\"$exists\":false}},{\"lock_owner\":\"%s\"}]}",
        static_cast<long long>(now), lock->owner);
    bson_t *filter = json_document(filter_json);
    bson_t update;
    bson_t values;
    bson_init(&update);
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &values);
    BSON_APPEND_UTF8(&values, "lock_owner", lock->owner);
    BSON_APPEND_INT64(&values, "lock_until",
                      now + static_cast<int64_t>(lock->lease_seconds));
    bson_append_document_end(&update, &values);
    bson_t options;
    bson_init(&options);
    BSON_APPEND_BOOL(&options, "upsert", true);
    bson_error_t error = {};
    bson_t reply;
    bson_init(&reply);
    const bool updated = mongoc_collection_update_one(
        state, filter, &update, &options, &reply, &error);
    bson_destroy(&reply);
    bson_destroy(&options);
    bson_destroy(&update);
    bson_destroy(filter);
    if (!updated && error.code == 11000) {
        fatal("другой экземпляр лингвистического обработчика уже запущен");
    }
    check_write(updated, &error, "обновление блокировки обработчика");
}

ProcessingLock acquire_processing_lock(mongoc_collection_t *state,
                                              uint64_t lease_seconds) {
    ProcessingLock lock = {};
    bson_oid_t identifier;
    bson_oid_init(&identifier, nullptr);
    bson_oid_to_string(&identifier, lock.owner);
    lock.lease_seconds = lease_seconds;
    update_processing_lock(state, &lock);
    return lock;
}

void release_processing_lock(mongoc_collection_t *state,
                                    const ProcessingLock *lock) {
    bson_t filter;
    bson_init(&filter);
    BSON_APPEND_UTF8(&filter, "_id", "processor_lock");
    BSON_APPEND_UTF8(&filter, "lock_owner", lock->owner);
    bson_t update;
    bson_t values;
    bson_init(&update);
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &values);
    BSON_APPEND_INT64(&values, "lock_until", 0);
    bson_append_document_end(&update, &values);
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$unset", &values);
    BSON_APPEND_UTF8(&values, "lock_owner", "");
    bson_append_document_end(&update, &values);
    bson_error_t error = {};
    bson_t reply;
    bson_init(&reply);
    check_write(mongoc_collection_update_one(state, &filter, &update, nullptr,
                                              &reply, &error),
                &error, "освобождение блокировки обработчика");
    bson_destroy(&reply);
    bson_destroy(&update);
    bson_destroy(&filter);
}

void prepare_frequencies(const MongoCollections &collections,
                         uint32_t parser_version) {
    mongoc_collection_t *state = collections.state;
    mongoc_collection_t *terms = collections.terms;
    mongoc_collection_t *tokens = collections.token_frequencies;
    mongoc_collection_t *lemmas = collections.lemma_frequencies;
    bool state_exists = false;
    bool inconsistent = frequency_state_dirty(state, &state_exists);
    const bool has_terms = collection_has_documents(terms);
    const bool has_tokens = collection_has_documents(tokens);
    const bool has_lemmas = collection_has_documents(lemmas);
    inconsistent = inconsistent || (!state_exists && (has_terms || has_tokens || has_lemmas));
    inconsistent = inconsistent || (has_terms && (!has_tokens || !has_lemmas));
    inconsistent = inconsistent || (!has_terms && (has_tokens || has_lemmas));
    if (inconsistent) {
        log_message("Частотные словари восстанавливаются из document_terms");
        rebuild_frequencies(terms, tokens, "tokens");
        rebuild_frequencies(terms, lemmas, "lemmas");
    }
    set_frequency_state(state, true, parser_version);
}

static void create_index(mongoc_collection_t *collection, const bson_t *keys,
                         bool unique) {
    bson_t options;
    bson_init(&options);
    if (unique) BSON_APPEND_BOOL(&options, "unique", true);
    mongoc_index_model_t *model = mongoc_index_model_new(
        keys, unique ? &options : nullptr);
    mongoc_index_model_t *models[] = {model};
    bson_error_t error = {};
    bson_t reply;
    bson_init(&reply);
    check_write(mongoc_collection_create_indexes_with_opts(
                    collection, models, 1, nullptr, &reply, &error),
                &error, "создание индекса MongoDB");
    bson_destroy(&reply);
    mongoc_index_model_destroy(model);
    bson_destroy(&options);
}

void create_indexes(const MongoCollections &collections) {
    mongoc_collection_t *processed = collections.processed;
    mongoc_collection_t *terms = collections.terms;
    mongoc_collection_t *tokens = collections.token_frequencies;
    mongoc_collection_t *lemmas = collections.lemma_frequencies;
    bson_t keys;
    bson_init(&keys);
    BSON_APPEND_INT32(&keys, "source", 1);
    BSON_APPEND_INT32(&keys, "source_id", 1);
    create_index(processed, &keys, true);
    bson_reinit(&keys);
    BSON_APPEND_INT32(&keys, "accepted", 1);
    BSON_APPEND_INT32(&keys, "source", 1);
    create_index(processed, &keys, false);
    bson_reinit(&keys);
    BSON_APPEND_INT32(&keys, "content_hash", 1);
    create_index(processed, &keys, false);
    bson_reinit(&keys);
    BSON_APPEND_INT32(&keys, "source", 1);
    create_index(terms, &keys, false);
    bson_reinit(&keys);
    BSON_APPEND_INT32(&keys, "count", -1);
    create_index(tokens, &keys, false);
    create_index(lemmas, &keys, false);
    bson_destroy(&keys);
}

void ping_database(mongoc_client_t *client, const char *database) {
    bson_t command;
    bson_t reply;
    bson_error_t error = {};
    bson_init(&command);
    bson_init(&reply);
    BSON_APPEND_INT32(&command, "ping", 1);
    check_write(mongoc_client_command_simple(client, database, &command, nullptr,
                                             &reply, &error),
                &error, "подключение к MongoDB");
    bson_destroy(&reply);
    bson_destroy(&command);
}


MongoCollections open_collections(mongoc_client_t *client,
                                  const Config &config) {
    MongoCollections collections;
    collections.documents = mongoc_client_get_collection(
        client, config.database.c_str(), config.documents.c_str());
    collections.processed = mongoc_client_get_collection(
        client, config.database.c_str(), config.processed.c_str());
    collections.terms = mongoc_client_get_collection(
        client, config.database.c_str(), config.document_terms.c_str());
    collections.token_frequencies = mongoc_client_get_collection(
        client, config.database.c_str(), config.token_frequencies.c_str());
    collections.lemma_frequencies = mongoc_client_get_collection(
        client, config.database.c_str(), config.lemma_frequencies.c_str());
    collections.state = mongoc_client_get_collection(
        client, config.database.c_str(), config.state.c_str());
    return collections;
}

void close_collections(MongoCollections *collections) {
    mongoc_collection_destroy(collections->state);
    mongoc_collection_destroy(collections->lemma_frequencies);
    mongoc_collection_destroy(collections->token_frequencies);
    mongoc_collection_destroy(collections->terms);
    mongoc_collection_destroy(collections->processed);
    mongoc_collection_destroy(collections->documents);
    *collections = {};
}
