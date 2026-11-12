#include "linguistics/language/lemmatizer.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using LemmaEntries = std::vector<std::pair<std::string, std::string>>;

struct LemmaDictionary {
    LemmaEntries exceptions;
    LemmaEntries overrides;
    std::vector<std::string> lemmas;
};

namespace {

struct LemmaPair {
    const char *form;
    const char *lemma;
};

const LemmaPair IRREGULAR_LEMMAS[] = {
    {"analyses", "analysis"}, {"are", "be"},         {"as", "as"},
    {"axes", "axe"},         {"been", "be"},        {"best", "good"},
    {"better", "good"},      {"children", "child"}, {"does", "do"},
    {"feet", "foot"},        {"gone", "go"},        {"had", "have"},
    {"has", "have"},         {"his", "his"},        {"indices", "index"},
    {"is", "be"},            {"men", "man"},        {"mice", "mouse"},
    {"news", "news"},        {"ran", "run"},        {"saw", "see"},
    {"series", "series"},    {"species", "species"},
    {"teeth", "tooth"},      {"this", "this"},      {"thus", "thus"},
    {"used", "use"},         {"using", "use"},      {"was", "be"},
    {"went", "go"},          {"were", "be"},        {"women", "woman"},
    {"worse", "bad"},        {"worst", "bad"},      {"yes", "yes"},
};

[[noreturn]] void fail(const char *message) {
    std::fprintf(stderr, "lemmatizer: %s\n", message);
    std::exit(EXIT_FAILURE);
}

bool ascii_word(const std::string &word) {
    for (const unsigned char character : word) {
        if (!std::isalpha(character)) return false;
    }
    return true;
}

bool vowel(char value) {
    return value == 'a' || value == 'e' || value == 'i' || value == 'o' ||
           value == 'u';
}

bool consonant(char value) {
    return value >= 'a' && value <= 'z' && !vowel(value);
}

const std::string *mapping_find(const LemmaEntries &entries,
                                const std::string &form) {
    const auto position = std::lower_bound(
        entries.begin(), entries.end(), form,
        [](const auto &entry, const std::string &value) {
            return entry.first < value;
        });
    if (position == entries.end() || position->first != form) return nullptr;
    return &position->second;
}

bool lemma_exists(const LemmaDictionary *dictionary,
                  const std::string &candidate) {
    return std::binary_search(dictionary->lemmas.begin(),
                              dictionary->lemmas.end(), candidate);
}

void sort_mappings(LemmaEntries *entries) {
    std::sort(entries->begin(), entries->end(),
              [](const auto &left, const auto &right) {
                  if (left.first != right.first) return left.first < right.first;
                  return left.second < right.second;
              });
    entries->erase(std::unique(entries->begin(), entries->end()),
                   entries->end());
}

void load_mappings(const char *path, LemmaEntries *entries) {
    if (path == nullptr || *path == '\0') return;
    std::ifstream file(path);
    if (!file) fail("cannot open lemma mappings");

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line.front() == '#') continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t separator = line.find('\t');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 == line.size()) {
            continue;
        }
        entries->emplace_back(line.substr(0, separator),
                              line.substr(separator + 1));
    }
    if (!file.eof()) fail("cannot read lemma mappings");
    sort_mappings(entries);
}

void load_lemmas(const char *path, std::vector<std::string> *lemmas) {
    if (path == nullptr || *path == '\0') return;
    std::ifstream file(path);
    if (!file) fail("cannot open lemma vocabulary");

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.front() != '#') lemmas->push_back(line);
    }
    if (!file.eof()) fail("cannot read lemma vocabulary");
    std::sort(lemmas->begin(), lemmas->end());
    lemmas->erase(std::unique(lemmas->begin(), lemmas->end()), lemmas->end());
}

std::string lemmatize_without_vocabulary(const std::string &word) {
    std::string lemma = word;
    const size_t length = word.size();
    if (length > 4 && word.ends_with("ies")) {
        lemma.replace(length - 3, 3, "y");
        return lemma;
    }
    if (length > 5 && word.ends_with("ing")) {
        lemma.resize(length - 3);
        if (lemma.size() >= 2 && lemma.back() == lemma[lemma.size() - 2] &&
            consonant(lemma.back())) {
            lemma.pop_back();
        } else if (lemma.back() == 'v' || lemma.ends_with("mak") ||
                   lemma.ends_with("creat")) {
            lemma.push_back('e');
        }
        return lemma;
    }
    if (length > 4 && word.ends_with("ed")) {
        lemma.resize(length - 2);
        if (lemma.size() >= 2 && lemma.back() == lemma[lemma.size() - 2] &&
            consonant(lemma.back())) {
            lemma.pop_back();
        } else if (lemma.back() == 'v' || lemma.ends_with("at") ||
                   lemma.ends_with("iz")) {
            lemma.push_back('e');
        }
        return lemma;
    }
    if (length > 4 &&
        (word.ends_with("sses") || word.ends_with("shes") ||
         word.ends_with("ches") || word.ends_with("xes") ||
         word.ends_with("zes") || word.ends_with("oes"))) {
        lemma.resize(length - 2);
        return lemma;
    }
    if (length > 3 && word.back() == 's' && !word.ends_with("ss") &&
        !word.ends_with("us") && !word.ends_with("is")) {
        lemma.pop_back();
    }
    return lemma;
}

std::string validated_verb_stem(const std::string &stem,
                                const LemmaDictionary *dictionary) {
    const bool doubled_consonant =
        stem.size() >= 2 && stem.back() == stem[stem.size() - 2] &&
        consonant(stem.back());
    if (doubled_consonant) {
        if (lemma_exists(dictionary, stem)) return stem;
        const std::string shortened = stem.substr(0, stem.size() - 1);
        if (lemma_exists(dictionary, shortened)) return shortened;
    }

    std::string with_silent_e = stem + 'e';
    const bool likely_silent_e =
        stem.size() >= 2 && vowel(stem[stem.size() - 2]) &&
        consonant(stem.back());
    if (likely_silent_e && lemma_exists(dictionary, with_silent_e)) {
        return with_silent_e;
    }
    if (lemma_exists(dictionary, stem)) return stem;
    if (lemma_exists(dictionary, with_silent_e)) return with_silent_e;
    return {};
}

std::string validated_suffix_lemma(const std::string &word,
                                   const LemmaDictionary *dictionary) {
    const size_t length = word.size();
    if (length > 4 && word.ends_with("ies")) {
        const std::string without_s = word.substr(0, length - 1);
        if (lemma_exists(dictionary, without_s)) return without_s;
        std::string with_y = word.substr(0, length - 3);
        with_y.push_back('y');
        if (lemma_exists(dictionary, with_y)) return with_y;
        return {};
    }
    if (length > 5 && word.ends_with("ing")) {
        return validated_verb_stem(word.substr(0, length - 3), dictionary);
    }
    if (length > 4 && word.ends_with("ed")) {
        return validated_verb_stem(word.substr(0, length - 2), dictionary);
    }
    if (length > 4 &&
        (word.ends_with("sses") || word.ends_with("shes") ||
         word.ends_with("ches") || word.ends_with("xes") ||
         word.ends_with("zes") || word.ends_with("oes"))) {
        const std::string candidate = word.substr(0, length - 2);
        if (lemma_exists(dictionary, candidate)) return candidate;
        return {};
    }
    if (length > 3 && word.back() == 's' && !word.ends_with("ss") &&
        !word.ends_with("us") && !word.ends_with("is")) {
        const std::string candidate = word.substr(0, length - 1);
        if (lemma_exists(dictionary, candidate)) return candidate;
    }
    return {};
}

std::string lemmatize_token(const std::string &word,
                            const LemmaDictionary *dictionary) {
    if (!ascii_word(word)) return word;

    if (const std::string *lemma =
            mapping_find(dictionary->overrides, word)) {
        return *lemma;
    }
    for (const LemmaPair &pair : IRREGULAR_LEMMAS) {
        if (word == pair.form) return pair.lemma;
    }
    if (const std::string *lemma =
            mapping_find(dictionary->exceptions, word)) {
        return *lemma;
    }
    if (dictionary->lemmas.empty()) {
        return lemmatize_without_vocabulary(word);
    }

    const std::string candidate = validated_suffix_lemma(word, dictionary);
    return candidate.empty() ? word : candidate;
}

}  // namespace

LemmaDictionary *lemma_dictionary_load(const char *exceptions_path,
                                       const char *lemmas_path,
                                       const char *overrides_path) {
    auto *dictionary = new LemmaDictionary;
    load_mappings(exceptions_path, &dictionary->exceptions);
    load_lemmas(lemmas_path, &dictionary->lemmas);
    load_mappings(overrides_path, &dictionary->overrides);
    return dictionary;
}

void lemma_dictionary_destroy(LemmaDictionary *dictionary) {
    delete dictionary;
}

LemmatizationResult lemmatize_tokens(const TokenList *tokens,
                                     const LemmaDictionary *dictionary) {
    const auto started = std::chrono::steady_clock::now();

    LemmatizationResult result;
    result.lemmas.reserve(tokens->size());
    for (const std::string &token : *tokens) {
        std::string lemma = lemmatize_token(token, dictionary);
        if (lemma != token) ++result.changed_token_count;
        result.lemmas.push_back(std::move(lemma));
    }

    const auto finished = std::chrono::steady_clock::now();
    result.elapsed_nanoseconds = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started)
            .count());
    return result;
}
