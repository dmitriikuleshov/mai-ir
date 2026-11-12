#include "linguistics/reporting/report.hpp"

#include "linguistics/language/pipeline.hpp"
#include "linguistics/reporting/report_writer.hpp"
#include "linguistics/reporting/statistics.hpp"

#include <bson/bson.h>
#include <mongoc/mongoc.h>

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

void fatal(const char *format, ...) {
    std::fprintf(stderr, "linguistics: ");
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fprintf(stderr, "\n");
    std::exit(EXIT_FAILURE);
}

void log_message(const char *format, ...) {
    const std::time_t now = std::time(nullptr);
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

const char *utf8_field(const bson_t *document, const char *name,
                       const char *fallback = "") {
    bson_iter_t iterator;
    if (bson_iter_init_find(&iterator, document, name) &&
        BSON_ITER_HOLDS_UTF8(&iterator)) {
        return bson_iter_utf8(&iterator, nullptr);
    }
    return fallback;
}

int64_t integer_field(const bson_t *document, const char *name,
                      int64_t fallback = 0) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, name)) return fallback;
    if (BSON_ITER_HOLDS_INT32(&iterator)) return bson_iter_int32(&iterator);
    if (BSON_ITER_HOLDS_INT64(&iterator)) return bson_iter_int64(&iterator);
    return fallback;
}

double number_field(const bson_t *document, const char *name) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, name)) return 0.0;
    if (BSON_ITER_HOLDS_DOUBLE(&iterator)) return bson_iter_double(&iterator);
    if (BSON_ITER_HOLDS_INT32(&iterator)) return bson_iter_int32(&iterator);
    if (BSON_ITER_HOLDS_INT64(&iterator)) {
        return static_cast<double>(bson_iter_int64(&iterator));
    }
    return 0.0;
}

void check_cursor(mongoc_cursor_t *cursor, const char *operation) {
    bson_error_t error = {};
    if (mongoc_cursor_error(cursor, &error)) {
        fatal("%s: %s", operation, error.message);
    }
}

bson_t *json_document(const char *json) {
    bson_error_t error = {};
    bson_t *document = bson_new_from_json(
        reinterpret_cast<const uint8_t *>(json), -1, &error);
    if (document == nullptr) fatal("ошибка BSON JSON: %s", error.message);
    return document;
}

void append_json_stage(bson_t *pipeline, uint32_t index, const char *json) {
    bson_t *stage = json_document(json);
    char key[32];
    const char *key_pointer = nullptr;
    bson_uint32_to_string(index, &key_pointer, key, sizeof(key));
    bson_append_document(pipeline, key_pointer, -1, stage);
    bson_destroy(stage);
}

bool embedded_document(const bson_t *document, const char *field,
                       bson_t *result) {
    bson_iter_t iterator;
    if (!bson_iter_init_find(&iterator, document, field) ||
        !BSON_ITER_HOLDS_DOCUMENT(&iterator)) {
        return false;
    }
    const uint8_t *data = nullptr;
    uint32_t length = 0;
    bson_iter_document(&iterator, &length, &data);
    return bson_init_static(result, data, length);
}

void calculate_rates(SourceStatistics *statistics) {
    statistics->average_token_length =
        statistics->tokens == 0
            ? 0.0
            : static_cast<double>(statistics->token_characters) /
                  static_cast<double>(statistics->tokens);
    statistics->kib_per_second =
        statistics->tokenization_seconds == 0.0
            ? 0.0
            : static_cast<double>(statistics->text_bytes) / 1024.0 /
                  statistics->tokenization_seconds;
}

std::vector<SourceStatistics> collect_source_statistics(
    mongoc_collection_t *processed) {
    bson_t pipeline;
    bson_init(&pipeline);
    append_json_stage(&pipeline, 0, "{\"$match\":{\"accepted\":true}}");
    append_json_stage(
        &pipeline, 1,
        "{\"$group\":{\"_id\":\"$source\",\"documents\":{\"$sum\":1},"
        "\"text_bytes\":{\"$sum\":\"$text_size\"},"
        "\"tokens\":{\"$sum\":\"$token_count\"},"
        "\"token_characters\":{\"$sum\":\"$token_characters\"},"
        "\"tokenization_seconds\":{\"$sum\":\"$tokenization_seconds\"}}}");
    append_json_stage(&pipeline, 2, "{\"$sort\":{\"_id\":1}}");

    mongoc_cursor_t *cursor = mongoc_collection_aggregate(
        processed, MONGOC_QUERY_NONE, &pipeline, nullptr, nullptr);
    std::vector<SourceStatistics> result;
    const bson_t *row = nullptr;
    while (mongoc_cursor_next(cursor, &row)) {
        SourceStatistics statistics;
        statistics.source = utf8_field(row, "_id");
        statistics.documents = integer_field(row, "documents");
        statistics.text_bytes = integer_field(row, "text_bytes");
        statistics.tokens = integer_field(row, "tokens");
        statistics.token_characters = integer_field(row, "token_characters");
        statistics.tokenization_seconds =
            number_field(row, "tokenization_seconds");
        calculate_rates(&statistics);
        result.push_back(statistics);
    }
    check_cursor(cursor, "расчёт статистики корпуса");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&pipeline);
    return result;
}

SourceStatistics total_statistics(
    const std::vector<SourceStatistics> &sources) {
    SourceStatistics total;
    total.source = "TOTAL";
    for (const SourceStatistics &source : sources) {
        total.documents += source.documents;
        total.text_bytes += source.text_bytes;
        total.tokens += source.tokens;
        total.token_characters += source.token_characters;
        total.tokenization_seconds += source.tokenization_seconds;
    }
    calculate_rates(&total);
    return total;
}

std::vector<PerformanceBucket> collect_performance(
    mongoc_collection_t *processed) {
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
    std::vector<PerformanceBucket> result;
    const bson_t *row = nullptr;
    while (mongoc_cursor_next(cursor, &row)) {
        PerformanceBucket bucket;
        bson_t bounds;
        if (embedded_document(row, "_id", &bounds)) {
            bucket.minimum_text_bytes = integer_field(&bounds, "min");
            bucket.maximum_text_bytes = integer_field(&bounds, "max");
        }
        bucket.documents = integer_field(row, "documents");
        bucket.tokens = integer_field(row, "tokens");
        bucket.tokenization_seconds =
            number_field(row, "tokenization_seconds");
        const int64_t text_bytes = integer_field(row, "text_bytes");
        bucket.kib_per_second =
            bucket.tokenization_seconds == 0.0
                ? 0.0
                : static_cast<double>(text_bytes) / 1024.0 /
                      bucket.tokenization_seconds;
        result.push_back(bucket);
    }
    check_cursor(cursor, "статистика производительности");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&pipeline);
    return result;
}

uint64_t collection_count(mongoc_collection_t *collection) {
    bson_error_t error = {};
    const int64_t count = mongoc_collection_estimated_document_count(
        collection, nullptr, nullptr, nullptr, &error);
    if (count < 0) fatal("подсчёт коллекции: %s", error.message);
    return static_cast<uint64_t>(count);
}

std::vector<FrequencyEntry> collect_frequencies(
    mongoc_collection_t *collection, size_t limit) {
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
    std::vector<FrequencyEntry> result;
    result.reserve(limit);
    const bson_t *row = nullptr;
    while (result.size() < limit && mongoc_cursor_next(cursor, &row)) {
        result.push_back(
            {utf8_field(row, "_id"),
             static_cast<uint64_t>(integer_field(row, "count"))});
    }
    check_cursor(cursor, "чтение популярных терминов");
    mongoc_cursor_destroy(cursor);
    bson_destroy(&options);
    bson_destroy(&filter);
    return result;
}

std::vector<uint64_t> collect_frequency_values(
    mongoc_collection_t *collection, size_t limit) {
    const std::vector<FrequencyEntry> entries =
        collect_frequencies(collection, limit);
    std::vector<uint64_t> values;
    values.reserve(entries.size());
    for (const FrequencyEntry &entry : entries) values.push_back(entry.count);
    return values;
}

std::string lemma_for(const char *word, const LemmaDictionary *dictionary,
                      size_t maximum_token_length) {
    const LinguisticResult result = linguistics_process(
        word, std::strlen(word), maximum_token_length, dictionary);
    return result.lemmas.empty() ? std::string{} : result.lemmas[0].term;
}

LemmatizationQuality evaluate_quality(const LemmaDictionary *dictionary,
                                      size_t maximum_token_length) {
    const char *forms[] = {"analyses", "indices", "systems", "queries",
                           "running", "retrieved", "databases", "models",
                           "children", "better"};
    const char *expected[] = {"analysis", "index", "system", "query", "run",
                              "retrieve", "database", "model", "child", "good"};
    size_t correct = 0;
    for (size_t index = 0; index < sizeof(forms) / sizeof(forms[0]); ++index) {
        if (lemma_for(forms[index], dictionary, maximum_token_length) ==
            expected[index]) {
            ++correct;
        }
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
    size_t queries = 0;
    for (const auto &group : groups) {
        size_t form_count = 0;
        while (form_count < 4 && group[form_count] != nullptr) ++form_count;
        std::vector<std::string> lemmas;
        lemmas.reserve(form_count);
        for (size_t index = 0; index < form_count; ++index) {
            lemmas.push_back(
                lemma_for(group[index], dictionary, maximum_token_length));
        }
        for (size_t query = 0; query < form_count; ++query) {
            size_t retrieved = 0;
            for (size_t candidate = 0; candidate < form_count; ++candidate) {
                if (lemmas[query] == lemmas[candidate]) ++retrieved;
            }
            baseline_recall_sum += 1.0 / static_cast<double>(form_count);
            lemma_recall_sum += static_cast<double>(retrieved) /
                                static_cast<double>(form_count);
            ++queries;
        }
    }

    LemmatizationQuality quality;
    quality.dictionary_accuracy =
        static_cast<double>(correct) /
        static_cast<double>(sizeof(forms) / sizeof(forms[0]));
    quality.baseline_macro_recall =
        baseline_recall_sum / static_cast<double>(queries);
    quality.lemmatized_macro_recall =
        lemma_recall_sum / static_cast<double>(queries);
    quality.lemmatized_macro_precision = 1.0;
    quality.ambiguous_axes_precision =
        lemma_for("axes", dictionary, maximum_token_length) == "axe" ? 0.5
                                                                       : 1.0;
    return quality;
}

void make_directories(const char *path) {
    std::string copy(path);
    for (size_t position = 1; position < copy.size(); ++position) {
        if (copy[position] != '/') continue;
        copy[position] = '\0';
        if (mkdir(copy.c_str(), 0755) != 0 && errno != EEXIST) {
            fatal("не удалось создать каталог %s", copy.c_str());
        }
        copy[position] = '/';
    }
    if (mkdir(copy.c_str(), 0755) != 0 && errno != EEXIST) {
        fatal("не удалось создать каталог %s", copy.c_str());
    }
}

ReportData collect_report_data(const Config &config, const Counters &counters,
                               const MongoCollections &collections,
                               const LemmaDictionary *dictionary,
                               double program_seconds) {
    ReportData report;
    report.generated_at = static_cast<int64_t>(std::time(nullptr));
    report.run = {config.parser_version, counters.scanned, counters.skipped,
                  counters.processed, counters.accepted, counters.rejected};
    report.sources = collect_source_statistics(collections.processed);
    report.corpus = total_statistics(report.sources);
    report.performance = collect_performance(collections.processed);
    report.token_vocabulary = collection_count(collections.token_frequencies);
    report.lemma_vocabulary = collection_count(collections.lemma_frequencies);
    report.top_tokens =
        collect_frequencies(collections.token_frequencies, config.top_terms);
    report.top_lemmas =
        collect_frequencies(collections.lemma_frequencies, config.top_terms);
    report.zipf_frequencies = collect_frequency_values(
        collections.lemma_frequencies, config.zipf_max_terms);
    report.mandelbrot =
        fit_mandelbrot(report.zipf_frequencies.data(),
                       report.zipf_frequencies.size(), config.zipf_fit_terms);
    report.quality =
        evaluate_quality(dictionary, config.maximum_token_length);
    report.program_wall_seconds = program_seconds;
    return report;
}

void log_report(const ReportData &report, const char *statistics_path,
                const char *plot_path) {
    log_message("Результаты обработки корпуса:");
    log_message("%-16s | %10s | %14s | %14s | %12s", "SOURCE", "DOCUMENTS",
                "TOKENS", "AVG TOKEN", "KiB/S");
    for (const SourceStatistics &source : report.sources) {
        log_message("%-16s | %10llu | %14llu | %14.2f | %12.1f",
                    source.source.c_str(),
                    static_cast<unsigned long long>(source.documents),
                    static_cast<unsigned long long>(source.tokens),
                    source.average_token_length, source.kib_per_second);
    }
    log_message("%-16s | %10llu | %14llu | %14.2f | %12.1f", "TOTAL",
                static_cast<unsigned long long>(report.corpus.documents),
                static_cast<unsigned long long>(report.corpus.tokens),
                report.corpus.average_token_length,
                report.corpus.kib_per_second);
    log_message("Словарь: %llu токенов → %llu лемм",
                static_cast<unsigned long long>(report.token_vocabulary),
                static_cast<unsigned long long>(report.lemma_vocabulary));
    log_message("Статистика: %s", statistics_path);
    log_message("График Ципфа: %s", plot_path);
}

}  // namespace

void write_report(const Config &config, const Counters &counters,
                  const MongoCollections &collections,
                  const LemmaDictionary *dictionary,
                  double program_seconds) {
    const ReportData report = collect_report_data(
        config, counters, collections, dictionary, program_seconds);

    make_directories(config.output_directory.c_str());
    const std::string statistics_path =
        config.output_directory + "/statistics.json";
    const std::string plot_path = config.output_directory + "/zipf.svg";

    write_zipf_svg(plot_path.c_str(), report.zipf_frequencies.data(),
                   report.zipf_frequencies.size(), &report.mandelbrot);
    if (!write_json_report(statistics_path.c_str(), report)) {
        fatal("не удалось записать %s", statistics_path.c_str());
    }
    log_report(report, statistics_path.c_str(), plot_path.c_str());
}
