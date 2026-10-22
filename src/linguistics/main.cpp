#include <bson/bson.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <mongoc/mongoc.h>

#include <cerrno>
#include <csignal>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

#include "core.hpp"

// The linguistic pipeline intentionally avoids STL containers. MongoDB and
// HTML are accessed through the C APIs of the official driver and libxml2.

struct Config {
    char uri[512];
    char database[128];
    char documents[128];
    char processed[128];
    char document_terms[128];
    char token_frequencies[128];
    char lemma_frequencies[128];
    char state[128];
    char lemma_dictionary[512];
    char output_directory[512];
    size_t batch_size;
    size_t minimum_words;
    size_t maximum_token_length;
    uint32_t parser_version;
    uint64_t max_documents;
    size_t progress_every;
    size_t zipf_max_terms;
    size_t zipf_fit_terms;
    size_t top_terms;
    uint64_t lock_lease_seconds;
};

struct TextBuffer {
    char *data;
    size_t size;
    size_t capacity;
};

struct Document {
    bson_t *raw;
    bson_t *processed;
    bson_t *terms;
    bson_oid_t id;
};

struct Delta {
    char *term;
    int64_t count;
};

struct DeltaArray {
    Delta *items;
    size_t size;
    size_t capacity;
};

struct Counters {
    uint64_t scanned;
    uint64_t skipped;
    uint64_t processed;
    uint64_t accepted;
    uint64_t rejected;
};

struct SourceStatistics {
    char source[32];
    uint64_t documents;
    uint64_t text_bytes;
    uint64_t tokens;
    uint64_t token_characters;
    uint64_t changed_tokens;
    double extraction_seconds;
    double tokenization_seconds;
    double lemmatization_seconds;
};

struct ProcessingLock {
    char owner[25];
    uint64_t lease_seconds;
};

static volatile std::sig_atomic_t stop_requested = 0;

static void request_stop(int) {
    stop_requested = 1;
}

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

static void *checked_realloc(void *pointer, size_t size) {
    void *result = std::realloc(pointer, size);
    if (result == nullptr) {
        fatal("недостаточно памяти");
    }
    return result;
}

static char *duplicate_text(const char *value) {
    const size_t length = std::strlen(value);
    char *result = static_cast<char *>(std::malloc(length + 1));
    if (result == nullptr) {
        fatal("недостаточно памяти");
    }
    std::memcpy(result, value, length + 1);
    return result;
}

static double monotonic_seconds() {
    timespec value = {};
    clock_gettime(CLOCK_MONOTONIC, &value);
    return static_cast<double>(value.tv_sec) +
           static_cast<double>(value.tv_nsec) / 1'000'000'000.0;
}

static char *trim(char *value) {
    while (*value == ' ' || *value == '\t') {
        ++value;
    }
    char *end = value + std::strlen(value);
    while (end > value &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
            end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    if ((*value == '\'' || *value == '"') && end - value >= 2 &&
        end[-1] == *value) {
        ++value;
        end[-1] = '\0';
    }
    return value;
}

static void copy_setting(char *target, size_t capacity, const char *value,
                         const char *name) {
    if (std::strlen(value) >= capacity) {
        fatal("слишком длинное значение %s", name);
    }
    std::strcpy(target, value);
}

static void resolve_config_path(char *value, size_t capacity,
                                const char *config_path) {
    if (*value == '\0' || *value == '/') return;
    const char *separator = std::strrchr(config_path, '/');
    if (separator == nullptr) return;
    const size_t directory_length =
        static_cast<size_t>(separator - config_path);
    char resolved[1024];
    const int written = directory_length == 0
                            ? std::snprintf(resolved, sizeof(resolved), "/%s", value)
                            : std::snprintf(resolved, sizeof(resolved), "%.*s/%s",
                                            static_cast<int>(directory_length),
                                            config_path, value);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(resolved)) {
        fatal("слишком длинный путь относительно файла конфигурации");
    }
    copy_setting(value, capacity, resolved, "path");
}

static uint64_t parse_positive(const char *value, const char *name) {
    char *end = nullptr;
    errno = 0;
    const unsigned long long result = std::strtoull(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || result == 0) {
        fatal("%s должен быть положительным целым числом", name);
    }
    return result;
}

static Config default_config() {
    Config config = {};
    copy_setting(config.uri, sizeof(config.uri), "mongodb://localhost:27017", "uri");
    copy_setting(config.database, sizeof(config.database), "mai_ir", "database");
    copy_setting(config.documents, sizeof(config.documents), "documents", "documents_collection");
    copy_setting(config.processed, sizeof(config.processed), "processed_documents", "processed_collection");
    copy_setting(config.document_terms, sizeof(config.document_terms), "document_terms", "document_terms_collection");
    copy_setting(config.token_frequencies, sizeof(config.token_frequencies), "token_frequencies", "token_frequencies_collection");
    copy_setting(config.lemma_frequencies, sizeof(config.lemma_frequencies), "lemma_frequencies", "lemma_frequencies_collection");
    copy_setting(config.state, sizeof(config.state), "linguistics_state", "linguistics_state_collection");
    copy_setting(config.lemma_dictionary, sizeof(config.lemma_dictionary), "resources/wordnet_exceptions.tsv", "lemma_dictionary");
    copy_setting(config.output_directory, sizeof(config.output_directory), "results", "output_directory");
    config.batch_size = 100;
    config.minimum_words = 500;
    config.maximum_token_length = 100;
    config.parser_version = 6;
    config.max_documents = 0;
    config.progress_every = 100;
    config.zipf_max_terms = 100000;
    config.zipf_fit_terms = 10000;
    config.top_terms = 30;
    config.lock_lease_seconds = 600;
    return config;
}

static Config load_config(const char *path) {
    Config config = default_config();
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        fatal("не удалось открыть конфигурацию %s", path);
    }
    char line[4096];
    char section[64] = "";
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char *content = trim(line);
        if (*content == '\0' || *content == '#') {
            continue;
        }
        const bool nested = line[0] == ' ' || line[0] == '\t';
        char *separator = std::strchr(content, ':');
        if (separator == nullptr) {
            continue;
        }
        *separator = '\0';
        char *key = trim(content);
        char *value = trim(separator + 1);
        if (!nested && *value == '\0') {
            copy_setting(section, sizeof(section), key, "section");
            continue;
        }
        if (std::strcmp(section, "db") == 0) {
            if (std::strcmp(key, "uri") == 0) copy_setting(config.uri, sizeof(config.uri), value, key);
            else if (std::strcmp(key, "database") == 0) copy_setting(config.database, sizeof(config.database), value, key);
            else if (std::strcmp(key, "documents_collection") == 0) copy_setting(config.documents, sizeof(config.documents), value, key);
            else if (std::strcmp(key, "processed_collection") == 0) copy_setting(config.processed, sizeof(config.processed), value, key);
            else if (std::strcmp(key, "document_terms_collection") == 0) copy_setting(config.document_terms, sizeof(config.document_terms), value, key);
            else if (std::strcmp(key, "token_frequencies_collection") == 0) copy_setting(config.token_frequencies, sizeof(config.token_frequencies), value, key);
            else if (std::strcmp(key, "lemma_frequencies_collection") == 0) copy_setting(config.lemma_frequencies, sizeof(config.lemma_frequencies), value, key);
            else if (std::strcmp(key, "linguistics_state_collection") == 0) copy_setting(config.state, sizeof(config.state), value, key);
        } else if (std::strcmp(section, "linguistics") == 0) {
            if (std::strcmp(key, "batch_size") == 0) config.batch_size = parse_positive(value, key);
            else if (std::strcmp(key, "minimum_words") == 0) config.minimum_words = parse_positive(value, key);
            else if (std::strcmp(key, "maximum_token_length") == 0) config.maximum_token_length = parse_positive(value, key);
            else if (std::strcmp(key, "parser_version") == 0) config.parser_version = parse_positive(value, key);
            else if (std::strcmp(key, "progress_every_documents") == 0) config.progress_every = parse_positive(value, key);
            else if (std::strcmp(key, "zipf_max_terms") == 0) config.zipf_max_terms = parse_positive(value, key);
            else if (std::strcmp(key, "zipf_fit_terms") == 0) config.zipf_fit_terms = parse_positive(value, key);
            else if (std::strcmp(key, "top_terms") == 0) config.top_terms = parse_positive(value, key);
            else if (std::strcmp(key, "lock_lease_seconds") == 0) config.lock_lease_seconds = parse_positive(value, key);
            else if (std::strcmp(key, "lemma_dictionary") == 0) copy_setting(config.lemma_dictionary, sizeof(config.lemma_dictionary), value, key);
            else if (std::strcmp(key, "output_directory") == 0) copy_setting(config.output_directory, sizeof(config.output_directory), value, key);
            else if (std::strcmp(key, "max_documents") == 0 && std::strcmp(value, "null") != 0) config.max_documents = parse_positive(value, key);
        }
    }
    if (std::ferror(file)) {
        std::fclose(file);
        fatal("ошибка чтения конфигурации");
    }
    std::fclose(file);
    resolve_config_path(config.lemma_dictionary,
                        sizeof(config.lemma_dictionary), path);
    resolve_config_path(config.output_directory,
                        sizeof(config.output_directory), path);
    return config;
}

static void text_reserve(TextBuffer *buffer, size_t extra) {
    if (buffer->size + extra + 1 <= buffer->capacity) {
        return;
    }
    size_t capacity = buffer->capacity == 0 ? 4096 : buffer->capacity;
    while (capacity < buffer->size + extra + 1) {
        capacity *= 2;
    }
    buffer->data = static_cast<char *>(checked_realloc(buffer->data, capacity));
    buffer->capacity = capacity;
}

static void text_append(TextBuffer *buffer, const char *value, size_t length) {
    text_reserve(buffer, length);
    std::memcpy(buffer->data + buffer->size, value, length);
    buffer->size += length;
    buffer->data[buffer->size] = '\0';
}

static void remove_xpath_nodes(xmlXPathContextPtr context, const char *xpath) {
    xmlXPathObjectPtr result = xmlXPathEvalExpression(
        reinterpret_cast<const xmlChar *>(xpath), context);
    if (result == nullptr || result->nodesetval == nullptr) {
        if (result != nullptr) xmlXPathFreeObject(result);
        return;
    }
    for (int index = result->nodesetval->nodeNr - 1; index >= 0; --index) {
        xmlNodePtr node = result->nodesetval->nodeTab[index];
        if (node != nullptr && node->parent != nullptr) {
            xmlUnlinkNode(node);
            xmlFreeNode(node);
        }
    }
    xmlXPathFreeObject(result);
}

static xmlNodePtr first_xpath_node(xmlXPathContextPtr context, const char *xpath) {
    xmlXPathObjectPtr result = xmlXPathEvalExpression(
        reinterpret_cast<const xmlChar *>(xpath), context);
    xmlNodePtr node = nullptr;
    if (result != nullptr && result->nodesetval != nullptr &&
        result->nodesetval->nodeNr > 0) {
        node = result->nodesetval->nodeTab[0];
    }
    if (result != nullptr) xmlXPathFreeObject(result);
    return node;
}

static char *normalize_whitespace(const char *input) {
    TextBuffer output = {};
    bool pending_space = false;
    const unsigned char *cursor = reinterpret_cast<const unsigned char *>(input);
    while (*cursor != '\0') {
        size_t width = 1;
        uint32_t codepoint = *cursor;
        if ((*cursor & 0xE0) == 0xC0 && cursor[1] != '\0') {
            width = 2;
            codepoint = ((*cursor & 0x1F) << 6) | (cursor[1] & 0x3F);
        } else if ((*cursor & 0xF0) == 0xE0 && cursor[1] != '\0' &&
                   cursor[2] != '\0') {
            width = 3;
            codepoint = ((*cursor & 0x0F) << 12) |
                        ((cursor[1] & 0x3F) << 6) | (cursor[2] & 0x3F);
        } else if ((*cursor & 0xF8) == 0xF0 && cursor[1] != '\0' &&
                   cursor[2] != '\0' && cursor[3] != '\0') {
            width = 4;
            codepoint = ((*cursor & 0x07) << 18) |
                        ((cursor[1] & 0x3F) << 12) |
                        ((cursor[2] & 0x3F) << 6) | (cursor[3] & 0x3F);
        }
        const bool whitespace =
            codepoint <= 0x20 || codepoint == 0x7F || codepoint == 0xA0 ||
            (codepoint >= 0x2000 && codepoint <= 0x200A) ||
            codepoint == 0x2028 || codepoint == 0x2029 ||
            codepoint == 0x202F || codepoint == 0x205F ||
            codepoint == 0x3000;
        if (whitespace) {
            pending_space = output.size > 0;
            cursor += width;
            continue;
        }
        if (pending_space) {
            text_append(&output, " ", 1);
            pending_space = false;
        }
        text_append(&output, reinterpret_cast<const char *>(cursor), width);
        cursor += width;
    }
    if (output.data == nullptr) return duplicate_text("");
    return output.data;
}

static char *extract_text(const char *raw_html, const char *source) {
    htmlDocPtr document = htmlReadMemory(
        raw_html, static_cast<int>(std::strlen(raw_html)), nullptr, "UTF-8",
        HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING |
            HTML_PARSE_NONET | HTML_PARSE_HUGE);
    if (document == nullptr) {
        return duplicate_text("");
    }
    xmlXPathContextPtr context = xmlXPathNewContext(document);
    if (context == nullptr) {
        xmlFreeDoc(document);
        fatal("не удалось создать XPath-контекст");
    }
    const char *root_xpath = "//main";
    if (std::strcmp(source, "wikipedia") == 0) root_xpath = "//*[@id='mw-content-text']";
    else if (std::strcmp(source, "arxiv") == 0) root_xpath = "//article[contains(concat(' ', normalize-space(@class), ' '), ' ltx_document ')]";
    xmlNodePtr root = first_xpath_node(context, root_xpath);
    if (root == nullptr && std::strcmp(root_xpath, "//main") != 0) {
        root = first_xpath_node(context, "//main");
    }
    if (root == nullptr) root = xmlDocGetRootElement(document);
    context->node = root;
    remove_xpath_nodes(context, ".//script|.//style|.//nav|.//aside|.//footer|.//header|.//form|.//svg|.//noscript|.//template|.//button");
    if (std::strcmp(source, "wikipedia") == 0) {
        remove_xpath_nodes(context, ".//*[contains(concat(' ', normalize-space(@class), ' '), ' mw-editsection ') or contains(concat(' ', normalize-space(@class), ' '), ' navbox ') or contains(concat(' ', normalize-space(@class), ' '), ' vertical-navbox ') or contains(concat(' ', normalize-space(@class), ' '), ' reflist ')]|.//*[@id='toc']");
    } else if (std::strcmp(source, "arxiv") == 0) {
        remove_xpath_nodes(context, ".//*[contains(concat(' ', normalize-space(@class), ' '), ' ltx_bibliography ') or contains(concat(' ', normalize-space(@class), ' '), ' ltx_page_footer ')]|.//annotation");
    } else if (std::strcmp(source, "microsoft_learn") == 0) {
        remove_xpath_nodes(context, ".//*[contains(concat(' ', normalize-space(@class), ' '), ' feedback-section ') or contains(concat(' ', normalize-space(@class), ' '), ' contributors-holder ') or contains(concat(' ', normalize-space(@class), ' '), ' recommendations ') or contains(concat(' ', normalize-space(@class), ' '), ' metadata ')]");
    }
    xmlChar *content = xmlNodeGetContent(root);
    char *text = normalize_whitespace(content == nullptr ? "" : reinterpret_cast<const char *>(content));
    if (content != nullptr) xmlFree(content);
    xmlXPathFreeContext(context);
    xmlFreeDoc(document);
    return text;
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

static double number_field(const bson_t *document, const char *name) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, name)) return 0.0;
    if (BSON_ITER_HOLDS_DOUBLE(&iterator)) return bson_iter_double(&iterator);
    if (BSON_ITER_HOLDS_INT32(&iterator)) return bson_iter_int32(&iterator);
    if (BSON_ITER_HOLDS_INT64(&iterator)) return static_cast<double>(bson_iter_int64(&iterator));
    return 0.0;
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
    if (array->size == array->capacity) {
        const size_t capacity = array->capacity == 0 ? 4096 : array->capacity * 2;
        array->items = static_cast<Delta *>(checked_realloc(
            array->items, capacity * sizeof(Delta)));
        array->capacity = capacity;
    }
    array->items[array->size++] = {duplicate_text(term), count};
}

static void deltas_from_frequency(DeltaArray *array,
                                  const FrequencyArray *frequencies,
                                  int sign) {
    for (size_t index = 0; index < frequencies->size; ++index) {
        delta_push(array, frequencies->items[index].term,
                   sign * static_cast<int64_t>(frequencies->items[index].count));
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

static int compare_deltas(const void *left, const void *right) {
    return std::strcmp(static_cast<const Delta *>(left)->term,
                       static_cast<const Delta *>(right)->term);
}

static size_t add_frequency_updates(mongoc_bulk_operation_t *bulk,
                                    DeltaArray *deltas) {
    std::qsort(deltas->items, deltas->size, sizeof(Delta), compare_deltas);
    size_t position = 0;
    size_t operations = 0;
    while (position < deltas->size) {
        size_t next = position + 1;
        int64_t total = deltas->items[position].count;
        while (next < deltas->size &&
               std::strcmp(deltas->items[position].term,
                           deltas->items[next].term) == 0) {
            total += deltas->items[next].count;
            ++next;
        }
        if (total != 0) {
            bson_t selector;
            bson_t update;
            bson_t increment;
            bson_init(&selector);
            bson_init(&update);
            BSON_APPEND_UTF8(&selector, "_id", deltas->items[position].term);
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

static void delta_array_destroy(DeltaArray *array) {
    for (size_t index = 0; index < array->size; ++index) {
        std::free(array->items[index].term);
    }
    std::free(array->items);
    *array = {};
}

static void append_frequency_array(bson_t *document, const char *field,
                                   const FrequencyArray *frequencies) {
    bson_t array;
    BSON_APPEND_ARRAY_BEGIN(document, field, &array);
    for (size_t index = 0; index < frequencies->size; ++index) {
        char key[32];
        const char *key_pointer = nullptr;
        bson_uint32_to_string(static_cast<uint32_t>(index), &key_pointer, key,
                              sizeof(key));
        bson_t pair;
        bson_append_array_begin(&array, key_pointer, -1, &pair);
        BSON_APPEND_UTF8(&pair, "0", frequencies->items[index].term);
        BSON_APPEND_INT64(&pair, "1",
                          static_cast<int64_t>(frequencies->items[index].count));
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

static void set_frequency_state(mongoc_collection_t *state, bool dirty,
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

static void update_processing_lock(mongoc_collection_t *state,
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

static ProcessingLock acquire_processing_lock(mongoc_collection_t *state,
                                              uint64_t lease_seconds) {
    ProcessingLock lock = {};
    bson_oid_t identifier;
    bson_oid_init(&identifier, nullptr);
    bson_oid_to_string(&identifier, lock.owner);
    lock.lease_seconds = lease_seconds;
    update_processing_lock(state, &lock);
    return lock;
}

static void release_processing_lock(mongoc_collection_t *state,
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

static void prepare_frequencies(mongoc_collection_t *state,
                                mongoc_collection_t *terms,
                                mongoc_collection_t *tokens,
                                mongoc_collection_t *lemmas,
                                uint32_t parser_version) {
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

static void create_indexes(mongoc_collection_t *processed,
                           mongoc_collection_t *terms,
                           mongoc_collection_t *tokens,
                           mongoc_collection_t *lemmas) {
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
        char *text = extract_text(raw_html, source);
        const double extraction_seconds = monotonic_seconds() - extraction_start;
        LinguisticResult result = linguistics_process(
            text, std::strlen(text), config->maximum_token_length, dictionary);
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
        add_processed_update(processed_bulk, document, config, text, &result,
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
        linguistics_result_destroy(&result);
        std::free(text);
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
    delta_array_destroy(&token_deltas);
    delta_array_destroy(&lemma_deltas);
    mongoc_bulk_operation_destroy(lemma_bulk);
    mongoc_bulk_operation_destroy(token_bulk);
    mongoc_bulk_operation_destroy(terms_bulk);
    mongoc_bulk_operation_destroy(processed_bulk);
}

static Counters process_documents(mongoc_collection_t *documents_collection,
                                  mongoc_collection_t *processed,
                                  mongoc_collection_t *terms,
                                  mongoc_collection_t *token_frequencies,
                                  mongoc_collection_t *lemma_frequencies,
                                  mongoc_collection_t *state,
                                  ProcessingLock *lock,
                                  const Config *config,
                                  const LemmaDictionary *dictionary) {
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
    Document *batch = static_cast<Document *>(
        std::calloc(config->batch_size, sizeof(Document)));
    if (batch == nullptr) fatal("недостаточно памяти");
    size_t batch_count = 0;
    const bson_t *item = nullptr;
    while (mongoc_cursor_next(cursor, &item)) {
        batch[batch_count].raw = bson_copy(item);
        if (!oid_field(item, &batch[batch_count].id)) {
            fatal("документ без ObjectId _id");
        }
        ++batch_count;
        if (batch_count == config->batch_size) {
            process_batch(batch, batch_count, processed, terms, token_frequencies,
                          lemma_frequencies, config, dictionary, &counters);
            update_processing_lock(state, lock);
            free_documents(batch, batch_count);
            std::memset(batch, 0, config->batch_size * sizeof(Document));
            batch_count = 0;
            if (stop_requested != 0) break;
            if (config->max_documents != 0 &&
                counters.processed >= config->max_documents) break;
        }
    }
    check_cursor(cursor, "чтение исходных документов");
    if (batch_count != 0 &&
        (config->max_documents == 0 || counters.processed < config->max_documents)) {
        process_batch(batch, batch_count, processed, terms, token_frequencies,
                      lemma_frequencies, config, dictionary, &counters);
        update_processing_lock(state, lock);
    }
    free_documents(batch, batch_count);
    std::free(batch);
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    remove_nonpositive(token_frequencies);
    remove_nonpositive(lemma_frequencies);
    return counters;
}

static size_t corpus_statistics(mongoc_collection_t *processed,
                                SourceStatistics *statistics,
                                size_t capacity) {
    bson_t pipeline;
    bson_init(&pipeline);
    append_json_stage(&pipeline, 0, "{\"$match\":{\"accepted\":true}}");
    append_json_stage(
        &pipeline, 1,
        "{\"$group\":{\"_id\":\"$source\",\"documents\":{\"$sum\":1},"
        "\"text_bytes\":{\"$sum\":\"$text_size\"},"
        "\"tokens\":{\"$sum\":\"$token_count\"},"
        "\"token_characters\":{\"$sum\":\"$token_characters\"},"
        "\"changed_tokens\":{\"$sum\":\"$changed_token_count\"},"
        "\"extraction_seconds\":{\"$sum\":\"$extraction_seconds\"},"
        "\"tokenization_seconds\":{\"$sum\":\"$tokenization_seconds\"},"
        "\"lemmatization_seconds\":{\"$sum\":\"$lemmatization_seconds\"}}}");
    append_json_stage(&pipeline, 2, "{\"$sort\":{\"_id\":1}}");
    mongoc_cursor_t *cursor = mongoc_collection_aggregate(
        processed, MONGOC_QUERY_NONE, &pipeline, nullptr, nullptr);
    const bson_t *row = nullptr;
    size_t size = 0;
    while (size < capacity && mongoc_cursor_next(cursor, &row)) {
        SourceStatistics *item = &statistics[size++];
        std::memset(item, 0, sizeof(*item));
        copy_setting(item->source, sizeof(item->source), utf8_field(row, "_id"),
                     "source");
        item->documents = integer_field(row, "documents");
        item->text_bytes = integer_field(row, "text_bytes");
        item->tokens = integer_field(row, "tokens");
        item->token_characters = integer_field(row, "token_characters");
        item->changed_tokens = integer_field(row, "changed_tokens");
        item->extraction_seconds = number_field(row, "extraction_seconds");
        item->tokenization_seconds = number_field(row, "tokenization_seconds");
        item->lemmatization_seconds = number_field(row, "lemmatization_seconds");
    }
    check_cursor(cursor, "расчёт статистики корпуса");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&pipeline);
    return size;
}

static uint64_t collection_count(mongoc_collection_t *collection) {
    bson_error_t error = {};
    const int64_t count = mongoc_collection_estimated_document_count(
        collection, nullptr, nullptr, nullptr, &error);
    if (count < 0) fatal("подсчёт коллекции: %s", error.message);
    return static_cast<uint64_t>(count);
}

static uint64_t *frequency_values(mongoc_collection_t *collection, size_t limit,
                                  size_t *size) {
    uint64_t *values = static_cast<uint64_t *>(
        std::malloc(limit * sizeof(uint64_t)));
    if (values == nullptr && limit != 0) fatal("недостаточно памяти");
    bson_t filter;
    bson_t options;
    bson_t sort;
    bson_t projection;
    bson_init(&filter);
    bson_init(&options);
    BSON_APPEND_INT64(&options, "limit", static_cast<int64_t>(limit));
    BSON_APPEND_DOCUMENT_BEGIN(&options, "sort", &sort);
    BSON_APPEND_INT32(&sort, "count", -1);
    bson_append_document_end(&options, &sort);
    BSON_APPEND_DOCUMENT_BEGIN(&options, "projection", &projection);
    BSON_APPEND_INT32(&projection, "count", 1);
    bson_append_document_end(&options, &projection);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, &filter, &options, nullptr);
    const bson_t *row = nullptr;
    *size = 0;
    while (*size < limit && mongoc_cursor_next(cursor, &row)) {
        values[(*size)++] = static_cast<uint64_t>(integer_field(row, "count"));
    }
    check_cursor(cursor, "чтение частот");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    return values;
}

struct MandelbrotFit {
    double c;
    double b;
    double s;
    double rmse;
};

static MandelbrotFit fit_mandelbrot(const uint64_t *frequencies, size_t size,
                                    size_t fit_terms) {
    MandelbrotFit best = {};
    best.rmse = HUGE_VAL;
    if (size > fit_terms) size = fit_terms;
    if (size < 2) return {};
    for (size_t candidate = 0; candidate <= 100; ++candidate) {
        const double b = candidate == 0
                             ? 0.0
                             : std::pow(10.0, -2.0 + 6.0 *
                                                       static_cast<double>(candidate - 1) /
                                                       99.0);
        double sum_x = 0.0;
        double sum_y = 0.0;
        double sum_xx = 0.0;
        double sum_xy = 0.0;
        for (size_t index = 0; index < size; ++index) {
            const double x = std::log(static_cast<double>(index + 1) + b);
            const double y = std::log(static_cast<double>(frequencies[index]));
            sum_x += x;
            sum_y += y;
            sum_xx += x * x;
            sum_xy += x * y;
        }
        const double count = static_cast<double>(size);
        const double denominator = count * sum_xx - sum_x * sum_x;
        if (denominator == 0.0) continue;
        const double slope = (count * sum_xy - sum_x * sum_y) / denominator;
        const double intercept = (sum_y - slope * sum_x) / count;
        double squared_error = 0.0;
        for (size_t index = 0; index < size; ++index) {
            const double predicted =
                intercept + slope *
                                std::log(static_cast<double>(index + 1) + b);
            const double error =
                std::log(static_cast<double>(frequencies[index])) - predicted;
            squared_error += error * error;
        }
        const double rmse = std::sqrt(squared_error / count);
        if (rmse < best.rmse) {
            best = {std::exp(intercept), b, -slope, rmse};
        }
    }
    return best;
}

static void make_directories(const char *path) {
    char copy[1024];
    copy_setting(copy, sizeof(copy), path, "output_directory");
    for (char *cursor = copy + 1; *cursor != '\0'; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
            fatal("не удалось создать каталог %s", copy);
        }
        *cursor = '/';
    }
    if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
        fatal("не удалось создать каталог %s", copy);
    }
}

static double plot_x(size_t rank, size_t maximum, double left, double width) {
    return left + std::log10(static_cast<double>(rank)) /
                      std::log10(static_cast<double>(maximum)) * width;
}

static double plot_y(double frequency, double maximum, double top,
                     double height) {
    const double normalized = std::log10(frequency) / std::log10(maximum);
    return top + (1.0 - normalized) * height;
}

static void write_polyline(FILE *file, const uint64_t *frequencies, size_t size,
                           const MandelbrotFit *fit, int mode) {
    const double left = 80.0;
    const double top = 35.0;
    const double width = 880.0;
    const double height = 500.0;
    const double maximum = static_cast<double>(frequencies[0]);
    const size_t points = size < 1600 ? size : 1600;
    const char *color = mode == 0 ? "#2563eb" : (mode == 1 ? "#dc2626" : "#16a34a");
    std::fprintf(file, "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"1.5\" points=\"", color);
    for (size_t point = 0; point < points; ++point) {
        size_t rank = 1 + point * (size - 1) / (points - 1);
        double frequency = static_cast<double>(frequencies[rank - 1]);
        if (mode == 1) frequency = maximum / static_cast<double>(rank);
        if (mode == 2) frequency = fit->c / std::pow(static_cast<double>(rank) + fit->b, fit->s);
        if (frequency < 1.0) frequency = 1.0;
        std::fprintf(file, "%.2f,%.2f ",
                     plot_x(rank, size, left, width),
                     plot_y(frequency, maximum, top, height));
    }
    std::fprintf(file, "\"/>\n");
}

static void write_zipf_svg(const char *path, const uint64_t *frequencies,
                           size_t size, const MandelbrotFit *fit) {
    if (size < 2) return;
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr) fatal("не удалось записать %s", path);
    std::fprintf(file,
                 "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" height=\"610\" viewBox=\"0 0 1000 610\">\n"
                 "<rect width=\"1000\" height=\"610\" fill=\"white\"/>\n"
                 "<text x=\"500\" y=\"22\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"18\">Распределение терминов по частотности</text>\n"
                 "<line x1=\"80\" y1=\"35\" x2=\"80\" y2=\"535\" stroke=\"black\"/><line x1=\"80\" y1=\"535\" x2=\"960\" y2=\"535\" stroke=\"black\"/>\n");
    write_polyline(file, frequencies, size, fit, 0);
    write_polyline(file, frequencies, size, fit, 1);
    write_polyline(file, frequencies, size, fit, 2);
    std::fprintf(file,
                 "<text x=\"500\" y=\"580\" text-anchor=\"middle\" font-family=\"sans-serif\">Ранг (логарифмическая шкала)</text>\n"
                 "<text x=\"18\" y=\"285\" transform=\"rotate(-90 18 285)\" text-anchor=\"middle\" font-family=\"sans-serif\">Частота (логарифмическая шкала)</text>\n"
                 "<text x=\"100\" y=\"558\" fill=\"#2563eb\" font-family=\"sans-serif\">корпус</text><text x=\"200\" y=\"558\" fill=\"#dc2626\" font-family=\"sans-serif\">Ципф</text><text x=\"280\" y=\"558\" fill=\"#16a34a\" font-family=\"sans-serif\">Мандельброт</text>\n</svg>\n");
    std::fclose(file);
}

static void json_string(FILE *file, const char *value) {
    std::fputc('"', file);
    for (const unsigned char *cursor = reinterpret_cast<const unsigned char *>(value);
         *cursor != '\0'; ++cursor) {
        if (*cursor == '"' || *cursor == '\\') std::fputc('\\', file);
        if (*cursor == '\n') std::fputs("\\n", file);
        else if (*cursor == '\r') std::fputs("\\r", file);
        else if (*cursor == '\t') std::fputs("\\t", file);
        else if (*cursor >= 0x20) std::fputc(*cursor, file);
    }
    std::fputc('"', file);
}

static bool embedded_document(const bson_t *document, const char *field,
                              bson_t *result) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, field) ||
        !BSON_ITER_HOLDS_DOCUMENT(&iterator)) return false;
    const uint8_t *data = nullptr;
    uint32_t length = 0;
    bson_iter_document(&iterator, &length, &data);
    return bson_init_static(result, data, length);
}

static void write_performance(FILE *file, mongoc_collection_t *processed) {
    bson_t pipeline;
    bson_init(&pipeline);
    append_json_stage(&pipeline, 0, "{\"$match\":{\"accepted\":true}}");
    append_json_stage(
        &pipeline, 1,
        "{\"$bucketAuto\":{\"groupBy\":\"$text_size\",\"buckets\":8,"
        "\"output\":{\"documents\":{\"$sum\":1},"
        "\"text_bytes\":{\"$sum\":\"$text_size\"},"
        "\"tokens\":{\"$sum\":\"$token_count\"},"
        "\"tokenization_seconds\":{\"$sum\":\"$tokenization_seconds\"}}}}");
    mongoc_cursor_t *cursor = mongoc_collection_aggregate(
        processed, MONGOC_QUERY_NONE, &pipeline, nullptr, nullptr);
    const bson_t *row = nullptr;
    bool first = true;
    std::fputs("  \"performance_by_text_size\": [\n", file);
    while (mongoc_cursor_next(cursor, &row)) {
        bson_t bounds;
        int64_t minimum = 0;
        int64_t maximum = 0;
        if (embedded_document(row, "_id", &bounds)) {
            minimum = integer_field(&bounds, "min");
            maximum = integer_field(&bounds, "max");
        }
        const int64_t bytes = integer_field(row, "text_bytes");
        const double seconds = number_field(row, "tokenization_seconds");
        if (!first) std::fputs(",\n", file);
        first = false;
        std::fprintf(file,
                     "    {\"minimum_text_bytes\": %lld, \"maximum_text_bytes\": %lld, "
                     "\"documents\": %lld, \"tokens\": %lld, "
                     "\"tokenization_seconds\": %.9f, \"kib_per_second\": %.3f}",
                     static_cast<long long>(minimum),
                     static_cast<long long>(maximum),
                     static_cast<long long>(integer_field(row, "documents")),
                     static_cast<long long>(integer_field(row, "tokens")), seconds,
                     seconds > 0.0 ? static_cast<double>(bytes) / 1024.0 / seconds : 0.0);
    }
    std::fputs("\n  ],\n", file);
    check_cursor(cursor, "статистика производительности");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&pipeline);
}

static void write_top_terms(FILE *file, mongoc_collection_t *collection,
                            size_t limit, const char *name, bool trailing_comma) {
    bson_t filter;
    bson_t options;
    bson_t sort;
    bson_init(&filter);
    bson_init(&options);
    BSON_APPEND_INT64(&options, "limit", static_cast<int64_t>(limit));
    BSON_APPEND_DOCUMENT_BEGIN(&options, "sort", &sort);
    BSON_APPEND_INT32(&sort, "count", -1);
    bson_append_document_end(&options, &sort);
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, &filter, &options, nullptr);
    const bson_t *row = nullptr;
    bool first = true;
    std::fprintf(file, "  \"%s\": [\n", name);
    while (mongoc_cursor_next(cursor, &row)) {
        if (!first) std::fputs(",\n", file);
        first = false;
        std::fputs("    {\"term\": ", file);
        json_string(file, utf8_field(row, "_id"));
        std::fprintf(file, ", \"count\": %lld}",
                     static_cast<long long>(integer_field(row, "count")));
    }
    std::fprintf(file, "\n  ]%s\n", trailing_comma ? "," : "");
    check_cursor(cursor, "чтение популярных терминов");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
}

static char *lemma_for(const char *word, const LemmaDictionary *dictionary,
                       size_t maximum_token_length) {
    LinguisticResult result = linguistics_process(
        word, std::strlen(word), maximum_token_length, dictionary);
    char *lemma = result.lemmas.size == 0
                      ? duplicate_text("")
                      : duplicate_text(result.lemmas.items[0].term);
    linguistics_result_destroy(&result);
    return lemma;
}

static void write_quality(FILE *file, const LemmaDictionary *dictionary,
                          size_t maximum_token_length) {
    const char *forms[] = {"analyses", "indices", "systems", "queries", "running",
                           "retrieved", "databases", "models", "children", "better"};
    const char *expected[] = {"analysis", "index", "system", "query", "run",
                              "retrieve", "database", "model", "child", "good"};
    size_t correct = 0;
    for (size_t index = 0; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        char *lemma = lemma_for(forms[index], dictionary, maximum_token_length);
        if (std::strcmp(lemma, expected[index]) == 0) ++correct;
        std::free(lemma);
    }

    const char *groups[][4] = {
        {"system", "systems", nullptr, nullptr},
        {"query", "queries", nullptr, nullptr},
        {"index", "indexes", "indexed", "indexing"},
        {"retrieve", "retrieves", "retrieved", "retrieving"},
        {"analysis", "analyses", nullptr, nullptr},
        {"model", "models", nullptr, nullptr},
        {"database", "databases", nullptr, nullptr},
        {"child", "children", nullptr, nullptr},
        {"run", "runs", "running", "ran"},
    };
    double baseline_recall_sum = 0.0;
    double lemma_recall_sum = 0.0;
    double lemma_precision_sum = 0.0;
    size_t queries = 0;
    for (size_t group = 0; group < sizeof(groups) / sizeof(groups[0]); ++group) {
        size_t form_count = 0;
        while (form_count < 4 && groups[group][form_count] != nullptr) ++form_count;
        char *lemmas[4] = {};
        for (size_t index = 0; index < form_count; ++index) {
            lemmas[index] = lemma_for(groups[group][index], dictionary,
                                      maximum_token_length);
        }
        for (size_t query = 0; query < form_count; ++query) {
            size_t retrieved = 0;
            for (size_t candidate = 0; candidate < form_count; ++candidate) {
                if (std::strcmp(lemmas[query], lemmas[candidate]) == 0) ++retrieved;
            }
            baseline_recall_sum += 1.0 / static_cast<double>(form_count);
            lemma_recall_sum += static_cast<double>(retrieved) /
                                static_cast<double>(form_count);
            lemma_precision_sum += 1.0;
            ++queries;
        }
        for (size_t index = 0; index < form_count; ++index) std::free(lemmas[index]);
    }
    char *axes = lemma_for("axes", dictionary, maximum_token_length);
    const double ambiguous_precision = std::strcmp(axes, "axe") == 0 ? 0.5 : 1.0;
    std::free(axes);
    std::fprintf(file,
                 "  \"lemmatization_quality\": {\n"
                 "    \"scope\": \"synthetic morphology check, not ranked search evaluation\",\n"
                 "    \"dictionary_accuracy\": %.6f,\n"
                 "    \"baseline_macro_recall\": %.6f,\n"
                 "    \"lemmatized_macro_recall\": %.6f,\n"
                 "    \"lemmatized_macro_precision\": %.6f,\n"
                 "    \"ambiguous_axes_precision\": %.6f\n"
                 "  },\n",
                 static_cast<double>(correct) /
                     static_cast<double>(sizeof(forms) / sizeof(forms[0])),
                 baseline_recall_sum / static_cast<double>(queries),
                 lemma_recall_sum / static_cast<double>(queries),
                 lemma_precision_sum / static_cast<double>(queries),
                 ambiguous_precision);
}

static void write_results(const Config *config, const Counters *counters,
                          mongoc_collection_t *processed,
                          mongoc_collection_t *token_frequencies,
                          mongoc_collection_t *lemma_frequencies,
                          const LemmaDictionary *dictionary,
                          double program_seconds) {
    SourceStatistics sources[8] = {};
    const size_t source_count = corpus_statistics(processed, sources, 8);
    SourceStatistics total = {};
    for (size_t index = 0; index < source_count; ++index) {
        total.documents += sources[index].documents;
        total.text_bytes += sources[index].text_bytes;
        total.tokens += sources[index].tokens;
        total.token_characters += sources[index].token_characters;
        total.changed_tokens += sources[index].changed_tokens;
        total.extraction_seconds += sources[index].extraction_seconds;
        total.tokenization_seconds += sources[index].tokenization_seconds;
        total.lemmatization_seconds += sources[index].lemmatization_seconds;
    }
    const uint64_t token_vocabulary = collection_count(token_frequencies);
    const uint64_t lemma_vocabulary = collection_count(lemma_frequencies);
    size_t frequency_count = 0;
    uint64_t *frequencies = frequency_values(
        lemma_frequencies, config->zipf_max_terms, &frequency_count);
    const MandelbrotFit fit = fit_mandelbrot(
        frequencies, frequency_count, config->zipf_fit_terms);
    make_directories(config->output_directory);
    char statistics_path[1024];
    char plot_path[1024];
    std::snprintf(statistics_path, sizeof(statistics_path), "%s/statistics.json",
                  config->output_directory);
    std::snprintf(plot_path, sizeof(plot_path), "%s/zipf.svg",
                  config->output_directory);
    write_zipf_svg(plot_path, frequencies, frequency_count, &fit);
    std::free(frequencies);

    FILE *file = std::fopen(statistics_path, "wb");
    if (file == nullptr) fatal("не удалось записать %s", statistics_path);
    std::fprintf(file,
                 "{\n  \"generated_at\": %lld,\n"
                 "  \"run\": {\"parser_version\": %u, \"scanned\": %llu, \"skipped\": %llu, "
                 "\"processed\": %llu, \"accepted\": %llu, \"rejected\": %llu},\n"
                 "  \"corpus\": {\n    \"documents\": %llu,\n"
                 "    \"text_bytes\": %llu,\n    \"tokens\": %llu,\n"
                 "    \"average_token_length\": %.9f,\n"
                 "    \"tokenization_seconds\": %.9f,\n"
                 "    \"tokenization_kib_per_second\": %.3f,\n"
                 "    \"token_vocabulary\": %llu,\n"
                 "    \"lemma_vocabulary\": %llu\n  },\n"
                 "  \"by_source\": [\n",
                 static_cast<long long>(std::time(nullptr)),
                 config->parser_version,
                 static_cast<unsigned long long>(counters->scanned),
                 static_cast<unsigned long long>(counters->skipped),
                 static_cast<unsigned long long>(counters->processed),
                 static_cast<unsigned long long>(counters->accepted),
                 static_cast<unsigned long long>(counters->rejected),
                 static_cast<unsigned long long>(total.documents),
                 static_cast<unsigned long long>(total.text_bytes),
                 static_cast<unsigned long long>(total.tokens),
                 total.tokens == 0 ? 0.0 : static_cast<double>(total.token_characters) /
                                                   static_cast<double>(total.tokens),
                 total.tokenization_seconds,
                 total.tokenization_seconds == 0.0
                     ? 0.0
                     : static_cast<double>(total.text_bytes) / 1024.0 /
                           total.tokenization_seconds,
                 static_cast<unsigned long long>(token_vocabulary),
                 static_cast<unsigned long long>(lemma_vocabulary));
    for (size_t index = 0; index < source_count; ++index) {
        if (index != 0) std::fputs(",\n", file);
        const SourceStatistics *item = &sources[index];
        std::fputs("    {\"source\": ", file);
        json_string(file, item->source);
        std::fprintf(file,
                     ", \"documents\": %llu, \"tokens\": %llu, "
                     "\"average_token_length\": %.9f, \"kib_per_second\": %.3f}",
                     static_cast<unsigned long long>(item->documents),
                     static_cast<unsigned long long>(item->tokens),
                     item->tokens == 0 ? 0.0 : static_cast<double>(item->token_characters) /
                                                        static_cast<double>(item->tokens),
                     item->tokenization_seconds == 0.0
                         ? 0.0
                         : static_cast<double>(item->text_bytes) / 1024.0 /
                               item->tokenization_seconds);
    }
    std::fputs("\n  ],\n", file);
    write_performance(file, processed);
    std::fprintf(file,
                 "  \"mandelbrot\": {\"c\": %.9f, \"b\": %.9f, "
                 "\"s\": %.9f, \"rmse_log\": %.9f},\n",
                 fit.c, fit.b, fit.s, fit.rmse);
    write_quality(file, dictionary, config->maximum_token_length);
    write_top_terms(file, token_frequencies, config->top_terms, "top_tokens", true);
    write_top_terms(file, lemma_frequencies, config->top_terms, "top_lemmas", true);
    std::fprintf(file, "  \"program_wall_seconds\": %.6f\n}\n", program_seconds);
    std::fclose(file);

    log_message("Результаты обработки корпуса:");
    log_message("%-16s | %10s | %14s | %14s | %12s", "SOURCE", "DOCUMENTS",
                "TOKENS", "AVG TOKEN", "KiB/S");
    for (size_t index = 0; index < source_count; ++index) {
        const SourceStatistics *item = &sources[index];
        log_message("%-16s | %10llu | %14llu | %14.2f | %12.1f", item->source,
                    static_cast<unsigned long long>(item->documents),
                    static_cast<unsigned long long>(item->tokens),
                    item->tokens == 0 ? 0.0 : static_cast<double>(item->token_characters) /
                                                       static_cast<double>(item->tokens),
                    item->tokenization_seconds == 0.0
                        ? 0.0
                        : static_cast<double>(item->text_bytes) / 1024.0 /
                              item->tokenization_seconds);
    }
    log_message("%-16s | %10llu | %14llu | %14.2f | %12.1f", "TOTAL",
                static_cast<unsigned long long>(total.documents),
                static_cast<unsigned long long>(total.tokens),
                total.tokens == 0 ? 0.0 : static_cast<double>(total.token_characters) /
                                                   static_cast<double>(total.tokens),
                total.tokenization_seconds == 0.0
                    ? 0.0
                    : static_cast<double>(total.text_bytes) / 1024.0 /
                          total.tokenization_seconds);
    log_message("Словарь: %llu токенов → %llu лемм",
                static_cast<unsigned long long>(token_vocabulary),
                static_cast<unsigned long long>(lemma_vocabulary));
    log_message("Статистика: %s", statistics_path);
    log_message("График Ципфа: %s", plot_path);
}

static void ping_database(mongoc_client_t *client, const char *database) {
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

#ifndef LINGUISTICS_TESTING
int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Использование: %s config.yaml\n", argv[0]);
        return EXIT_FAILURE;
    }
    const double program_started = monotonic_seconds();
    const Config config = load_config(argv[1]);
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
    mongoc_init();
    xmlInitParser();
    mongoc_client_t *client = mongoc_client_new(config.uri);
    if (client == nullptr) fatal("некорректный URI MongoDB");
    mongoc_client_set_appname(client, "mai-ir-linguistics-cpp");
    ping_database(client, config.database);
    mongoc_collection_t *documents = mongoc_client_get_collection(
        client, config.database, config.documents);
    mongoc_collection_t *processed = mongoc_client_get_collection(
        client, config.database, config.processed);
    mongoc_collection_t *terms = mongoc_client_get_collection(
        client, config.database, config.document_terms);
    mongoc_collection_t *token_frequencies = mongoc_client_get_collection(
        client, config.database, config.token_frequencies);
    mongoc_collection_t *lemma_frequencies = mongoc_client_get_collection(
        client, config.database, config.lemma_frequencies);
    mongoc_collection_t *state = mongoc_client_get_collection(
        client, config.database, config.state);
    LemmaDictionary *dictionary = lemma_dictionary_load(config.lemma_dictionary);

    ProcessingLock lock = acquire_processing_lock(state,
                                                  config.lock_lease_seconds);
    prepare_frequencies(state, terms, token_frequencies, lemma_frequencies,
                        config.parser_version);
    create_indexes(processed, terms, token_frequencies, lemma_frequencies);
    const Counters counters = process_documents(
        documents, processed, terms, token_frequencies, lemma_frequencies,
        state, &lock, &config, dictionary);
    set_frequency_state(state, false, config.parser_version);
    write_results(&config, &counters, processed, token_frequencies,
                  lemma_frequencies, dictionary,
                  monotonic_seconds() - program_started);
    release_processing_lock(state, &lock);

    lemma_dictionary_destroy(dictionary);
    mongoc_collection_destroy(state);
    mongoc_collection_destroy(lemma_frequencies);
    mongoc_collection_destroy(token_frequencies);
    mongoc_collection_destroy(terms);
    mongoc_collection_destroy(processed);
    mongoc_collection_destroy(documents);
    mongoc_client_destroy(client);
    xmlCleanupParser();
    mongoc_cleanup();
    if (stop_requested != 0) {
        log_message("Обработка остановлена по сигналу; данные сохранены согласованно");
        return 130;
    }
    return EXIT_SUCCESS;
}
#endif

