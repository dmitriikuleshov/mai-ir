#ifndef MAI_IR_LINGUISTICS_REPORT_WRITER_HPP
#define MAI_IR_LINGUISTICS_REPORT_WRITER_HPP

#include "linguistics/reporting/statistics.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct RunStatistics {
    uint32_t parser_version = 0;
    uint64_t scanned = 0;
    uint64_t skipped = 0;
    uint64_t processed = 0;
    uint64_t accepted = 0;
    uint64_t rejected = 0;
};

struct SourceStatistics {
    std::string source;
    uint64_t documents = 0;
    uint64_t text_bytes = 0;
    uint64_t tokens = 0;
    uint64_t token_characters = 0;
    double tokenization_seconds = 0.0;
    double average_token_length = 0.0;
    double kib_per_second = 0.0;
};

struct PerformanceBucket {
    int64_t minimum_text_bytes = 0;
    int64_t maximum_text_bytes = 0;
    uint64_t documents = 0;
    uint64_t tokens = 0;
    double tokenization_seconds = 0.0;
    double kib_per_second = 0.0;
};

struct FrequencyEntry {
    std::string term;
    uint64_t count = 0;
};

struct LemmatizationQuality {
    double dictionary_accuracy = 0.0;
    double baseline_macro_recall = 0.0;
    double lemmatized_macro_recall = 0.0;
    double lemmatized_macro_precision = 0.0;
    double ambiguous_axes_precision = 0.0;
};

struct ReportData {
    int64_t generated_at = 0;
    RunStatistics run;
    SourceStatistics corpus;
    std::vector<SourceStatistics> sources;
    std::vector<PerformanceBucket> performance;
    uint64_t token_vocabulary = 0;
    uint64_t lemma_vocabulary = 0;
    MandelbrotFit mandelbrot = {};
    LemmatizationQuality quality;
    std::vector<FrequencyEntry> top_tokens;
    std::vector<FrequencyEntry> top_lemmas;
    std::vector<uint64_t> zipf_frequencies;
    double program_wall_seconds = 0.0;
};

bool write_json_report(const char *path, const ReportData &report);

#endif
