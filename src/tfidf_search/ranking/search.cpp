#include "tfidf_search/ranking/search.hpp"

#include "boolean_search/index/index_reader.hpp"
#include "tfidf_search/ranking/weights.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct QueryTermWeight {
    std::string term;
    double weight;
};

void set_error(char *error, size_t error_size, const char *message) {
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

bool result_before(const RankedDocument &left, const RankedDocument &right) {
    return left.score > right.score ||
           (left.score == right.score &&
            left.document_id < right.document_id);
}

}  // namespace

TfidfSearchResult tfidf_search(TfidfIndex *index, const char *query,
                               size_t maximum_token_length,
                               size_t result_limit,
                               const LemmaDictionary *dictionary,
                               char *error, size_t error_size) {
    if (error != nullptr && error_size != 0) error[0] = '\0';
    TfidfSearchResult result{};
    if (index == nullptr || query == nullptr) {
        set_error(error, error_size, "не передан индекс или текст запроса");
        return result;
    }

    const LinguisticResult normalized = linguistics_process(
        query, std::strlen(query), maximum_token_length, dictionary);
    if (normalized.lemmas.empty()) {
        set_error(error, error_size,
                  "запрос не содержит индексируемых терминов");
        return result;
    }

    const uint32_t document_count = index->boolean_index.header.document_count;
    std::vector<QueryTermWeight> query_weights;
    double squared_query_norm = 0.0;
    for (const TermFrequency &term : normalized.lemmas) {
        boolean_index_format::LexiconEntry entry{};
        if (!boolean_index_find_term(&index->boolean_index, term.term.c_str(),
                                     &entry)) {
            continue;
        }
        const double weight = logarithmic_term_frequency(term.count) *
                              inverse_document_frequency(
                                  document_count, entry.postings_count);
        if (weight > 0.0) {
            query_weights.push_back({term.term, weight});
            squared_query_norm += weight * weight;
        }
    }
    if (query_weights.empty() || squared_query_norm == 0.0) return result;

    const double query_norm = std::sqrt(squared_query_norm);
    std::vector<double> scores(document_count, 0.0);
    std::vector<uint32_t> touched_documents;
    for (const QueryTermWeight &query_term : query_weights) {
        const double normalized_query_weight = query_term.weight / query_norm;
        const WeightedPostingList postings = load_weighted_postings(
            &index->boolean_index, query_term.term.c_str());
        if (postings.empty()) {
            set_error(error, error_size,
                      "не удалось прочитать posting list из индекса");
            return {};
        }
        for (const WeightedPosting &posting : postings) {
            if (posting.document_id >= index->document_norms.size()) {
                set_error(error, error_size,
                          "posting list содержит неверный docID");
                return {};
            }
            const double document_norm =
                index->document_norms[posting.document_id];
            if (document_norm == 0.0) continue;
            const double contribution =
                normalized_query_weight *
                logarithmic_term_frequency(posting.term_frequency) /
                document_norm;
            if (scores[posting.document_id] == 0.0) {
                touched_documents.push_back(posting.document_id);
            }
            scores[posting.document_id] += contribution;
        }
    }

    result.documents.reserve(touched_documents.size());
    for (uint32_t document_id : touched_documents) {
        const double score = scores[document_id];
        if (score > 0.0 && std::isfinite(score)) {
            result.documents.push_back({document_id, score});
        }
    }
    std::sort(result.documents.begin(), result.documents.end(), result_before);
    result.matched_documents = result.documents.size();
    if (result.documents.size() > result_limit) {
        result.documents.resize(result_limit);
    }
    return result;
}
