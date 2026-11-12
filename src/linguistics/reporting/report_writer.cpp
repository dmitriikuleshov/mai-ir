#include "linguistics/reporting/report_writer.hpp"

#include <cstdio>

namespace {

void write_json_string(FILE *file, const char *value) {
    std::fputc('"', file);
    for (const unsigned char *cursor =
             reinterpret_cast<const unsigned char *>(value);
         *cursor != '\0'; ++cursor) {
        if (*cursor == '"' || *cursor == '\\') std::fputc('\\', file);
        if (*cursor == '\n') {
            std::fputs("\\n", file);
        } else if (*cursor == '\r') {
            std::fputs("\\r", file);
        } else if (*cursor == '\t') {
            std::fputs("\\t", file);
        } else if (*cursor >= 0x20) {
            std::fputc(*cursor, file);
        }
    }
    std::fputc('"', file);
}

void write_sources(FILE *file, const std::vector<SourceStatistics> &sources) {
    std::fputs("  \"by_source\": [\n", file);
    for (size_t index = 0; index < sources.size(); ++index) {
        if (index != 0) std::fputs(",\n", file);
        const SourceStatistics &source = sources[index];
        std::fputs("    {\"source\": ", file);
        write_json_string(file, source.source.c_str());
        std::fprintf(file,
                     ", \"documents\": %llu, \"tokens\": %llu, "
                     "\"average_token_length\": %.9f, \"kib_per_second\": %.3f}",
                     static_cast<unsigned long long>(source.documents),
                     static_cast<unsigned long long>(source.tokens),
                     source.average_token_length, source.kib_per_second);
    }
    std::fputs("\n  ],\n", file);
}

void write_performance(FILE *file,
                       const std::vector<PerformanceBucket> &performance) {
    std::fputs("  \"performance_by_text_size\": [\n", file);
    for (size_t index = 0; index < performance.size(); ++index) {
        if (index != 0) std::fputs(",\n", file);
        const PerformanceBucket &bucket = performance[index];
        std::fprintf(file,
                     "    {\"minimum_text_bytes\": %lld, "
                     "\"maximum_text_bytes\": %lld, \"documents\": %llu, "
                     "\"tokens\": %llu, \"tokenization_seconds\": %.9f, "
                     "\"kib_per_second\": %.3f}",
                     static_cast<long long>(bucket.minimum_text_bytes),
                     static_cast<long long>(bucket.maximum_text_bytes),
                     static_cast<unsigned long long>(bucket.documents),
                     static_cast<unsigned long long>(bucket.tokens),
                     bucket.tokenization_seconds, bucket.kib_per_second);
    }
    std::fputs("\n  ],\n", file);
}

void write_frequencies(FILE *file, const char *name,
                       const std::vector<FrequencyEntry> &frequencies) {
    std::fprintf(file, "  \"%s\": [\n", name);
    for (size_t index = 0; index < frequencies.size(); ++index) {
        if (index != 0) std::fputs(",\n", file);
        std::fputs("    {\"term\": ", file);
        write_json_string(file, frequencies[index].term.c_str());
        std::fprintf(file, ", \"count\": %llu}",
                     static_cast<unsigned long long>(frequencies[index].count));
    }
    std::fputs("\n  ],\n", file);
}

}  // namespace

bool write_json_report(const char *path, const ReportData &report) {
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr) return false;

    std::fprintf(
        file,
        "{\n  \"generated_at\": %lld,\n"
        "  \"run\": {\"parser_version\": %u, \"scanned\": %llu, "
        "\"skipped\": %llu, \"processed\": %llu, \"accepted\": %llu, "
        "\"rejected\": %llu},\n"
        "  \"corpus\": {\n    \"documents\": %llu,\n"
        "    \"text_bytes\": %llu,\n    \"tokens\": %llu,\n"
        "    \"average_token_length\": %.9f,\n"
        "    \"tokenization_seconds\": %.9f,\n"
        "    \"tokenization_kib_per_second\": %.3f,\n"
        "    \"token_vocabulary\": %llu,\n"
        "    \"lemma_vocabulary\": %llu\n  },\n",
        static_cast<long long>(report.generated_at), report.run.parser_version,
        static_cast<unsigned long long>(report.run.scanned),
        static_cast<unsigned long long>(report.run.skipped),
        static_cast<unsigned long long>(report.run.processed),
        static_cast<unsigned long long>(report.run.accepted),
        static_cast<unsigned long long>(report.run.rejected),
        static_cast<unsigned long long>(report.corpus.documents),
        static_cast<unsigned long long>(report.corpus.text_bytes),
        static_cast<unsigned long long>(report.corpus.tokens),
        report.corpus.average_token_length,
        report.corpus.tokenization_seconds, report.corpus.kib_per_second,
        static_cast<unsigned long long>(report.token_vocabulary),
        static_cast<unsigned long long>(report.lemma_vocabulary));

    write_sources(file, report.sources);
    write_performance(file, report.performance);
    std::fprintf(file,
                 "  \"mandelbrot\": {\"c\": %.9f, \"b\": %.9f, "
                 "\"s\": %.9f, \"rmse_log\": %.9f},\n",
                 report.mandelbrot.c, report.mandelbrot.b,
                 report.mandelbrot.s, report.mandelbrot.rmse);
    std::fprintf(file,
                 "  \"lemmatization_quality\": {\n"
                 "    \"scope\": \"synthetic morphology check, not ranked search evaluation\",\n"
                 "    \"dictionary_accuracy\": %.6f,\n"
                 "    \"baseline_macro_recall\": %.6f,\n"
                 "    \"lemmatized_macro_recall\": %.6f,\n"
                 "    \"lemmatized_macro_precision\": %.6f,\n"
                 "    \"ambiguous_axes_precision\": %.6f\n"
                 "  },\n",
                 report.quality.dictionary_accuracy,
                 report.quality.baseline_macro_recall,
                 report.quality.lemmatized_macro_recall,
                 report.quality.lemmatized_macro_precision,
                 report.quality.ambiguous_axes_precision);
    write_frequencies(file, "top_tokens", report.top_tokens);
    write_frequencies(file, "top_lemmas", report.top_lemmas);
    std::fprintf(file, "  \"program_wall_seconds\": %.6f\n}\n",
                 report.program_wall_seconds);

    const bool success = std::ferror(file) == 0;
    return std::fclose(file) == 0 && success;
}
