#include "linguistics/processing/document_processor.hpp"

#include "linguistics/language/pipeline.hpp"
#include "linguistics/processing/html_extractor.hpp"

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace {

struct Document {
    bson_t *raw = nullptr;
    bson_t *processed = nullptr;
    bson_t *terms = nullptr;
    bson_oid_t id{};
};

struct Delta {
    std::string term;
    int64_t count = 0;
};

using DeltaArray = std::vector<Delta>;

volatile std::sig_atomic_t stop_requested = 0;

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

static double monotonic_seconds() {
    const auto elapsed =
        std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(elapsed).count();
}

static const char *utf8_field(const bson_t *document, const char *name,
                              const char *fallback = "") {
    bson_iter_t iterator;
    if (bson_iter_init_find(&iterator, document, name) &&
        BSON_ITER_HOLDS_UTF8(&iterator)) {
        return bson_iter_utf8(&iterator, nullptr);
    }
    return fallback;
}

static int64_t integer_field(const bson_t *document, const char *name,
                             int64_t fallback = 0) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, name)) return fallback;
    if (BSON_ITER_HOLDS_INT32(&iterator)) return bson_iter_int32(&iterator);
    if (BSON_ITER_HOLDS_INT64(&iterator)) return bson_iter_int64(&iterator);
    return fallback;
}

static bool oid_field(const bson_t *document, bson_oid_t *oid) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, "_id") ||
        !BSON_ITER_HOLDS_OID(&iterator)) return false;
    *oid = *bson_iter_oid(&iterator);
    return true;
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

static bson_t *batch_id_filter(Document *documents, size_t count) {
    bson_t *filter = bson_new();
    bson_t condition;
    bson_t array;
    BSON_APPEND_DOCUMENT_BEGIN(filter, "_id", &condition);
    BSON_APPEND_ARRAY_BEGIN(&condition, "$in", &array);
    for (size_t index = 0; index < count; ++index) {
        char key[32];
        const char *key_pointer = nullptr;
        bson_uint32_to_string(static_cast<uint32_t>(index), &key_pointer, key,
                              sizeof(key));
        bson_append_oid(&array, key_pointer, -1, &documents[index].id);
    }
    bson_append_array_end(&condition, &array);
    bson_append_document_end(filter, &condition);
    return filter;
}

static Document *find_document(Document *documents, size_t count,
                               const bson_oid_t *oid) {
    for (size_t index = 0; index < count; ++index) {
        if (bson_oid_equal(&documents[index].id, oid)) return &documents[index];
    }
    return nullptr;
}

static void load_related(mongoc_collection_t *collection, Document *documents,
                         size_t count, bool processed) {
    bson_t *filter = batch_id_filter(documents, count);
    bson_t opts;
    bson_init(&opts);
    bson_t projection;
    BSON_APPEND_DOCUMENT_BEGIN(&opts, "projection", &projection);
    BSON_APPEND_INT32(&projection, "_id", 1);
    if (processed) {
        BSON_APPEND_INT32(&projection, "content_hash", 1);
        BSON_APPEND_INT32(&projection, "parser_version", 1);
    } else {
        BSON_APPEND_INT32(&projection, "tokens", 1);
        BSON_APPEND_INT32(&projection, "lemmas", 1);
    }
    bson_append_document_end(&opts, &projection);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, filter, &opts, nullptr);
    const bson_t *item = nullptr;
    while (mongoc_cursor_next(cursor, &item)) {
        bson_oid_t oid;
        if (!oid_field(item, &oid)) continue;
        Document *document = find_document(documents, count, &oid);
        if (document == nullptr) continue;
        if (processed) document->processed = bson_copy(item);
        else document->terms = bson_copy(item);
    }
    check_cursor(cursor, "чтение связанных документов");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&opts);
    bson_destroy(filter);
}

static bool document_is_current(const Document *document, const Config *config) {
    if (document->processed == nullptr) return false;
    return integer_field(document->processed, "parser_version") ==
               config->parser_version &&
           std::strcmp(utf8_field(document->processed, "content_hash"),
                       utf8_field(document->raw, "content_hash")) == 0;
}

static void delta_push(DeltaArray *array, const char *term, int64_t count) {
    if (count == 0) return;
    array->push_back({term, count});
}

static void deltas_from_frequency(DeltaArray *array,
                                  const FrequencyArray *frequencies,
                                  int sign) {
    for (const TermFrequency &frequency : *frequencies) {
        delta_push(array, frequency.term.c_str(),
                   sign * static_cast<int64_t>(frequency.count));
    }
}

static void deltas_from_bson(DeltaArray *array, const bson_t *document,
                             const char *field, int sign) {
    if (document == nullptr) return;
    bson_iter_t outer;
    if (!bson_iter_init_find(&outer, document, field) ||
        !BSON_ITER_HOLDS_ARRAY(&outer)) return;
    bson_iter_t entries;
    if (!bson_iter_recurse(&outer, &entries)) return;
    while (bson_iter_next(&entries)) {
        if (!BSON_ITER_HOLDS_ARRAY(&entries)) continue;
        bson_iter_t pair;
        if (!bson_iter_recurse(&entries, &pair) || !bson_iter_next(&pair) ||
            !BSON_ITER_HOLDS_UTF8(&pair)) continue;
        const char *term = bson_iter_utf8(&pair, nullptr);
        if (!bson_iter_next(&pair) || !BSON_ITER_HOLDS_NUMBER(&pair)) continue;
        const int64_t count = BSON_ITER_HOLDS_INT64(&pair)
                                  ? bson_iter_int64(&pair)
                                  : bson_iter_int32(&pair);
        delta_push(array, term, sign * count);
    }
}

static size_t add_frequency_updates(mongoc_bulk_operation_t *bulk,
                                    DeltaArray *deltas) {
    std::sort(deltas->begin(), deltas->end(),
              [](const Delta &left, const Delta &right) {
                  return left.term < right.term;
              });
    size_t position = 0;
    size_t operations = 0;
    while (position < deltas->size()) {
        size_t next = position + 1;
        int64_t total = (*deltas)[position].count;
        while (next < deltas->size() &&
               (*deltas)[position].term == (*deltas)[next].term) {
            total += (*deltas)[next].count;
            ++next;
        }
        if (total != 0) {
            bson_t selector;
            bson_t update;
            bson_t increment;
            bson_init(&selector);
            bson_init(&update);
            BSON_APPEND_UTF8(&selector, "_id",
                             (*deltas)[position].term.c_str());
            BSON_APPEND_DOCUMENT_BEGIN(&update, "$inc", &increment);
            BSON_APPEND_INT64(&increment, "count", total);
            bson_append_document_end(&update, &increment);
            mongoc_bulk_operation_update_one(bulk, &selector, &update, true);
            ++operations;
            bson_destroy(&update);
            bson_destroy(&selector);
        }
        position = next;
    }
    return operations;
}

static void append_frequency_array(bson_t *document, const char *field,
                                   const FrequencyArray *frequencies) {
    bson_t array;
    BSON_APPEND_ARRAY_BEGIN(document, field, &array);
    for (size_t index = 0; index < frequencies->size(); ++index) {
        char key[32];
        const char *key_pointer = nullptr;
        bson_uint32_to_string(static_cast<uint32_t>(index), &key_pointer, key,
                              sizeof(key));
        bson_t pair;
        bson_append_array_begin(&array, key_pointer, -1, &pair);
        BSON_APPEND_UTF8(&pair, "0", (*frequencies)[index].term.c_str());
        BSON_APPEND_INT64(&pair, "1",
                          static_cast<int64_t>((*frequencies)[index].count));
        bson_append_array_end(&array, &pair);
    }
    bson_append_array_end(document, &array);
}

static void append_source_id(bson_t *target, const bson_t *source) {
    bson_iter_t iterator;
    if (bson_iter_init_find(&iterator, source, "source_id")) {
        BSON_APPEND_VALUE(target, "source_id", bson_iter_value(&iterator));
    }
}

static void add_processed_update(mongoc_bulk_operation_t *bulk,
                                 const Document *document, const Config *config,
                                 const char *text, const LinguisticResult *result,
                                 bool accepted, double extraction_seconds) {
    bson_t selector;
    bson_t update;
    bson_t values;
    bson_init(&selector);
    bson_init(&update);
    BSON_APPEND_OID(&selector, "_id", &document->id);
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &values);
    BSON_APPEND_UTF8(&values, "source", utf8_field(document->raw, "source"));
    append_source_id(&values, document->raw);
    BSON_APPEND_UTF8(&values, "url", utf8_field(document->raw, "url"));
    BSON_APPEND_UTF8(&values, "title", utf8_field(document->raw, "title"));
    BSON_APPEND_UTF8(&values, "content_hash", utf8_field(document->raw, "content_hash"));
    BSON_APPEND_INT32(&values, "parser_version", config->parser_version);
    BSON_APPEND_BOOL(&values, "accepted", accepted);
    BSON_APPEND_INT64(&values, "text_size", static_cast<int64_t>(std::strlen(text)));
    BSON_APPEND_INT64(&values, "word_count", static_cast<int64_t>(result->token_count));
    BSON_APPEND_INT64(&values, "token_count", static_cast<int64_t>(result->token_count));
    BSON_APPEND_INT64(&values, "token_characters", static_cast<int64_t>(result->token_characters));
    BSON_APPEND_INT64(&values, "changed_token_count", static_cast<int64_t>(result->changed_token_count));
    BSON_APPEND_DOUBLE(&values, "extraction_seconds", extraction_seconds);
    BSON_APPEND_DOUBLE(&values, "tokenization_seconds", static_cast<double>(result->tokenization_nanoseconds) / 1e9);
    BSON_APPEND_DOUBLE(&values, "lemmatization_seconds", static_cast<double>(result->lemmatization_nanoseconds) / 1e9);
    BSON_APPEND_INT64(&values, "processed_at", static_cast<int64_t>(std::time(nullptr)));
    if (accepted) {
        BSON_APPEND_UTF8(&values, "text", text);
        BSON_APPEND_NULL(&values, "rejection_reason");
    } else {
        BSON_APPEND_UTF8(&values, "rejection_reason", "too_short");
    }
    bson_append_document_end(&update, &values);
    if (!accepted) {
        bson_t unset;
        BSON_APPEND_DOCUMENT_BEGIN(&update, "$unset", &unset);
        BSON_APPEND_UTF8(&unset, "text", "");
        bson_append_document_end(&update, &unset);
    }
    mongoc_bulk_operation_update_one(bulk, &selector, &update, true);
    bson_destroy(&update);
    bson_destroy(&selector);
}

static void add_terms_update(mongoc_bulk_operation_t *bulk,
                             const Document *document,
                             const LinguisticResult *result, bool accepted) {
    bson_t selector;
    bson_init(&selector);
    BSON_APPEND_OID(&selector, "_id", &document->id);
    if (!accepted) {
        mongoc_bulk_operation_remove_one(bulk, &selector);
        bson_destroy(&selector);
        return;
    }
    bson_t update;
    bson_t values;
    bson_init(&update);
    BSON_APPEND_DOCUMENT_BEGIN(&update, "$set", &values);
    BSON_APPEND_UTF8(&values, "source", utf8_field(document->raw, "source"));
    append_source_id(&values, document->raw);
    BSON_APPEND_UTF8(&values, "content_hash", utf8_field(document->raw, "content_hash"));
    append_frequency_array(&values, "tokens", &result->tokens);
    append_frequency_array(&values, "lemmas", &result->lemmas);
    bson_append_document_end(&update, &values);
    mongoc_bulk_operation_update_one(bulk, &selector, &update, true);
    bson_destroy(&update);
    bson_destroy(&selector);
}

static void execute_bulk(mongoc_bulk_operation_t *bulk, const char *operation) {
    bson_error_t error = {};
    bson_t reply;
    bson_init(&reply);
    const uint32_t result = mongoc_bulk_operation_execute(bulk, &reply, &error);
    bson_destroy(&reply);
    if (result == 0) fatal("%s: %s", operation, error.message);
}

static void free_documents(Document *documents, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (documents[index].raw != nullptr) bson_destroy(documents[index].raw);
        if (documents[index].processed != nullptr) bson_destroy(documents[index].processed);
        if (documents[index].terms != nullptr) bson_destroy(documents[index].terms);
    }
}

static bson_t *json_document(const char *json) {
    bson_error_t error{};
    bson_t *document = bson_new_from_json(
        reinterpret_cast<const uint8_t *>(json), -1, &error);
    if (document == nullptr) fatal("ошибка BSON JSON: %s", error.message);
    return document;
}

static void remove_nonpositive(mongoc_collection_t *collection) {
    bson_t *filter = json_document("{\"count\":{\"$lte\":0}}");
    bson_error_t error = {};
    bson_t reply;
    bson_init(&reply);
    check_write(mongoc_collection_delete_many(collection, filter, nullptr,
                                               &reply, &error),
                &error, "удаление нулевых частот");
    bson_destroy(&reply);
    bson_destroy(filter);
}

static void process_batch(Document *documents, size_t count,
                          mongoc_collection_t *processed,
                          mongoc_collection_t *terms,
                          mongoc_collection_t *token_frequencies,
                          mongoc_collection_t *lemma_frequencies,
                          const Config *config,
                          const LemmaDictionary *dictionary,
                          Counters *counters) {
    load_related(processed, documents, count, true);
    bool has_pending = false;
    for (size_t index = 0; index < count; ++index) {
        ++counters->scanned;
        if (document_is_current(&documents[index], config)) ++counters->skipped;
        else has_pending = true;
    }
    if (!has_pending) return;
    load_related(terms, documents, count, false);

    bson_t bulk_options;
    bson_init(&bulk_options);
    BSON_APPEND_BOOL(&bulk_options, "ordered", false);
    mongoc_bulk_operation_t *processed_bulk =
        mongoc_collection_create_bulk_operation_with_opts(processed, &bulk_options);
    mongoc_bulk_operation_t *terms_bulk =
        mongoc_collection_create_bulk_operation_with_opts(terms, &bulk_options);
    mongoc_bulk_operation_t *token_bulk =
        mongoc_collection_create_bulk_operation_with_opts(token_frequencies,
                                                           &bulk_options);
    mongoc_bulk_operation_t *lemma_bulk =
        mongoc_collection_create_bulk_operation_with_opts(lemma_frequencies,
                                                           &bulk_options);
    bson_destroy(&bulk_options);
    DeltaArray token_deltas = {};
    DeltaArray lemma_deltas = {};
    size_t pending_count = 0;

    for (size_t index = 0; index < count; ++index) {
        Document *document = &documents[index];
        if (document_is_current(document, config)) continue;
        if (config->max_documents != 0 &&
            counters->processed >= config->max_documents) break;
        const char *source = utf8_field(document->raw, "source");
        const char *raw_html = utf8_field(document->raw, "raw_html");
        const double extraction_start = monotonic_seconds();
        std::string text = extract_text(raw_html, source);
        const double extraction_seconds = monotonic_seconds() - extraction_start;
        LinguisticResult result = linguistics_process(
            text.data(), text.size(), config->maximum_token_length, dictionary);
        const bool accepted = result.token_count >= config->minimum_words;

        deltas_from_bson(&token_deltas, document->terms, "tokens", -1);
        deltas_from_bson(&lemma_deltas, document->terms, "lemmas", -1);
        if (accepted) {
            deltas_from_frequency(&token_deltas, &result.tokens, 1);
            deltas_from_frequency(&lemma_deltas, &result.lemmas, 1);
            ++counters->accepted;
        } else {
            ++counters->rejected;
        }
        add_processed_update(processed_bulk, document, config, text.c_str(), &result,
                             accepted, extraction_seconds);
        add_terms_update(terms_bulk, document, &result, accepted);
        ++counters->processed;
        ++pending_count;
        if (counters->processed % config->progress_every == 0) {
            log_message("Обработано: %llu | принято: %llu | короче %zu слов: %llu | без изменений: %llu",
                        static_cast<unsigned long long>(counters->processed),
                        static_cast<unsigned long long>(counters->accepted),
                        config->minimum_words,
                        static_cast<unsigned long long>(counters->rejected),
                        static_cast<unsigned long long>(counters->skipped));
        }
    }

    if (pending_count != 0) {
        const size_t token_operations =
            add_frequency_updates(token_bulk, &token_deltas);
        const size_t lemma_operations =
            add_frequency_updates(lemma_bulk, &lemma_deltas);
        if (token_operations != 0) {
            execute_bulk(token_bulk, "обновление частот токенов");
        }
        if (lemma_operations != 0) {
            execute_bulk(lemma_bulk, "обновление частот лемм");
        }
        // document_terms is the recoverable source of truth for global
        // frequencies. processed_documents is written last and therefore acts
        // as the commit marker used by document_is_current().
        execute_bulk(terms_bulk, "запись термов документов");
        execute_bulk(processed_bulk, "запись обработанных документов");
    }
    mongoc_bulk_operation_destroy(lemma_bulk);
    mongoc_bulk_operation_destroy(token_bulk);
    mongoc_bulk_operation_destroy(terms_bulk);
    mongoc_bulk_operation_destroy(processed_bulk);
}

}  // namespace

void request_processing_stop(int) {
    stop_requested = 1;
}

bool processing_stop_requested() {
    return stop_requested != 0;
}

Counters process_documents(const MongoCollections &collections,
                           ProcessingLock *lock, const Config &config_value,
                           const LemmaDictionary *dictionary) {
    mongoc_collection_t *documents_collection = collections.documents;
    mongoc_collection_t *processed = collections.processed;
    mongoc_collection_t *terms = collections.terms;
    mongoc_collection_t *token_frequencies = collections.token_frequencies;
    mongoc_collection_t *lemma_frequencies = collections.lemma_frequencies;
    mongoc_collection_t *state = collections.state;
    const Config *config = &config_value;
    Counters counters = {};
    bson_t filter;
    bson_t options;
    bson_t projection;
    bson_t sort;
    bson_init(&filter);
    bson_init(&options);
    BSON_APPEND_INT32(&options, "batchSize", static_cast<int32_t>(config->batch_size));
    BSON_APPEND_DOCUMENT_BEGIN(&options, "projection", &projection);
    const char *fields[] = {"_id", "source", "source_id", "url", "title",
                            "raw_html", "content_hash"};
    for (const char *field : fields) BSON_APPEND_INT32(&projection, field, 1);
    bson_append_document_end(&options, &projection);
    BSON_APPEND_DOCUMENT_BEGIN(&options, "sort", &sort);
    BSON_APPEND_INT32(&sort, "_id", 1);
    bson_append_document_end(&options, &sort);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        documents_collection, &filter, &options, nullptr);
    std::vector<Document> batch(config->batch_size);
    size_t batch_count = 0;
    const bson_t *item = nullptr;
    while (mongoc_cursor_next(cursor, &item)) {
        batch[batch_count].raw = bson_copy(item);
        if (!oid_field(item, &batch[batch_count].id)) {
            fatal("документ без ObjectId _id");
        }
        ++batch_count;
        if (batch_count == config->batch_size) {
            process_batch(batch.data(), batch_count, processed, terms, token_frequencies,
                          lemma_frequencies, config, dictionary, &counters);
            update_processing_lock(state, lock);
            free_documents(batch.data(), batch_count);
            std::fill(batch.begin(), batch.end(), Document{});
            batch_count = 0;
            if (stop_requested != 0) break;
            if (config->max_documents != 0 &&
                counters.processed >= config->max_documents) break;
        }
    }
    check_cursor(cursor, "чтение исходных документов");
    if (batch_count != 0 &&
        (config->max_documents == 0 || counters.processed < config->max_documents)) {
        process_batch(batch.data(), batch_count, processed, terms, token_frequencies,
                      lemma_frequencies, config, dictionary, &counters);
        update_processing_lock(state, lock);
    }
    free_documents(batch.data(), batch_count);
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    remove_nonpositive(token_frequencies);
    remove_nonpositive(lemma_frequencies);
    return counters;
}
