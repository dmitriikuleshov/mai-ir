#include <gtest/gtest.h>

#include "linguistics/language/pipeline.hpp"

#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

uint64_t frequency(const FrequencyArray &frequencies, const char *term) {
    for (const TermFrequency &frequency : frequencies) {
        if (frequency.term == term) return frequency.count;
    }
    return 0;
}

class LemmatizationTest : public testing::Test {
protected:
    void SetUp() override {
        dictionary_ = lemma_dictionary_load(
            MAI_IR_SOURCE_DIR "/resources/wordnet_exceptions.tsv",
            MAI_IR_SOURCE_DIR "/resources/wordnet_lemmas.txt",
            MAI_IR_SOURCE_DIR "/resources/technical_lemma_overrides.tsv");
        ASSERT_NE(dictionary_, nullptr);
    }

    void TearDown() override { lemma_dictionary_destroy(dictionary_); }

    LinguisticResult lemmatize(const char *text) const {
        return linguistics_process(text, std::strlen(text), 100, dictionary_);
    }

    LemmatizationResult lemmatize_terms(
        std::initializer_list<const char *> terms) const {
        TokenList tokens;
        tokens.reserve(terms.size());
        for (const char *term : terms) tokens.emplace_back(term);
        return lemmatize_tokens(&tokens, dictionary_);
    }

    LemmatizationResult lemmatize_terms(const TokenList &tokens) const {
        return lemmatize_tokens(&tokens, dictionary_);
    }

private:
    LemmaDictionary *dictionary_ = nullptr;
};

// Проверяется сведение регулярных форм множественного числа к единственному.
TEST_F(LemmatizationTest, HandlesRegularPluralForms) {
    const LinguisticResult result =
        lemmatize("queries query databases classes boxes");

    EXPECT_EQ(frequency(result.lemmas, "query"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "database"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "class"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "box"), 1U);
}

// Проверяется обработка окончания -ing и восстановление основы слова.
TEST_F(LemmatizationTest, HandlesIngEndingAndRestoresStem) {
    const LinguisticResult result =
        lemmatize("running making creating retrieving indexing");

    EXPECT_EQ(frequency(result.lemmas, "run"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "make"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "create"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "retrieve"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "index"), 1U);
}

// Проверяется обработка окончания -ed и восстановление основы слова.
TEST_F(LemmatizationTest, HandlesEdEndingAndRestoresStem) {
    const LinguisticResult result =
        lemmatize("stopped retrieved indexed normalized");

    EXPECT_EQ(frequency(result.lemmas, "stop"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "retrieve"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "index"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "normalize"), 1U);
}

// Проверяется встроенная таблица неправильных словоформ.
TEST_F(LemmatizationTest, HandlesBuiltInIrregularForms) {
    const LinguisticResult result =
        lemmatize("children men mice indices went better");

    EXPECT_EQ(frequency(result.lemmas, "child"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "man"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "mouse"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "index"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "go"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "good"), 1U);
}

// Проверяется применение исключений из словаря Princeton WordNet.
TEST_F(LemmatizationTest, UsesPrincetonWordNetExceptions) {
    const LinguisticResult result = lemmatize("abaci alumni oxen geese");

    EXPECT_EQ(frequency(result.lemmas, "abacus"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "alumnus"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "ox"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "goose"), 1U);
}

// Проверяется сохранение разных технических термов без искажений.
TEST_F(LemmatizationTest, DoesNotDistortTechnicalTerms) {
    const LinguisticResult result =
        lemmatize("c++ c# .net node.js sha-256 3.14");
    const char *terms[] = {"c++", "c#", ".net", "node.js", "sha-256", "3.14"};

    for (const char *term : terms) {
        SCOPED_TRACE(term);
        EXPECT_EQ(frequency(result.lemmas, term), 1U);
    }
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется сохранение и подсчёт технических термов в смешанном тексте.
TEST_F(LemmatizationTest, PreservesRepeatedTechnicalTermsInMixedText) {
    const LinguisticResult result = lemmatize(
        "C++ c++ Node.js NODE.JS SHA-256 sha‑256 queries databases");

    EXPECT_EQ(result.token_count, 8U);
    EXPECT_EQ(frequency(result.lemmas, "c++"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "node.js"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "sha-256"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "query"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "database"), 1U);
    EXPECT_EQ(result.changed_token_count, 2U);
}

// Проверяется объединение нескольких словоформ и суммирование их частот.
TEST_F(LemmatizationTest, MergesSeveralInflectedFormsIntoOneLemma) {
    const LinguisticResult result = lemmatize(
        "query queries retrieve retrieved retrieving "
        "index indices indexed indexing");

    EXPECT_EQ(frequency(result.lemmas, "query"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "retrieve"), 3U);
    EXPECT_EQ(frequency(result.lemmas, "index"), 4U);
    EXPECT_EQ(result.lemmas.size(), 3U);
}

// Проверяется сведение всех основных форм неправильного глагола be.
TEST_F(LemmatizationTest, CombinesFormsOfIrregularVerbBe) {
    const LinguisticResult result =
        lemmatize("be is are was were been");

    EXPECT_EQ(result.token_count, 6U);
    EXPECT_EQ(result.lemmas.size(), 1U);
    EXPECT_EQ(frequency(result.lemmas, "be"), 6U);
    EXPECT_EQ(result.changed_token_count, 5U);
}

// Проверяется обработка неправильных сравнительных и превосходных форм.
TEST_F(LemmatizationTest, HandlesComparativeAndSuperlativeIrregularForms) {
    const LinguisticResult result =
        lemmatize("good better best bad worse worst");

    EXPECT_EQ(frequency(result.lemmas, "good"), 3U);
    EXPECT_EQ(frequency(result.lemmas, "bad"), 3U);
    EXPECT_EQ(result.changed_token_count, 4U);
}

// Проверяется, что окончания -ss, -us и -is не обрезаются ошибочно.
TEST_F(LemmatizationTest, LeavesProtectedSingularEndingsUnchanged) {
    const LinguisticResult result =
        lemmatize("news series species glass status analysis");
    const char *expected[] = {"news", "series", "species",
                              "glass", "status", "analysis"};

    for (const char *term : expected) {
        SCOPED_TRACE(term);
        EXPECT_EQ(frequency(result.lemmas, term), 1U);
    }
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется лемматизация после удаления притяжательного окончания.
TEST_F(LemmatizationTest, LemmatizesWordsAfterPossessiveRemoval) {
    const LinguisticResult result =
        lemmatize("children's databases query's");

    EXPECT_EQ(frequency(result.tokens, "children"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "child"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "database"), 1U);
    EXPECT_EQ(frequency(result.lemmas, "query"), 1U);
    EXPECT_EQ(result.changed_token_count, 2U);
}

// Проверяется объединение исключений WordNet с уже базовыми формами.
TEST_F(LemmatizationTest, MergesDictionaryExceptionsWithBaseForms) {
    const LinguisticResult result =
        lemmatize("abaci abacus alumni alumnus oxen ox geese goose");

    EXPECT_EQ(frequency(result.lemmas, "abacus"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "alumnus"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "ox"), 2U);
    EXPECT_EQ(frequency(result.lemmas, "goose"), 2U);
    EXPECT_EQ(result.changed_token_count, 4U);
}

// Проверяется точный подсчёт всех изменённых при лемматизации словоформ.
TEST_F(LemmatizationTest, CountsEveryChangedWordForm) {
    const LinguisticResult result =
        lemmatize("queries queries query database");

    EXPECT_EQ(result.token_count, 4U);
    EXPECT_EQ(result.changed_token_count, 2U);
    EXPECT_EQ(frequency(result.lemmas, "query"), 3U);
}

// Проверяется сведение нескольких обычных форм с окончанием -s к единственному числу.
TEST_F(LemmatizationTest, HandlesSeveralSimpleSPluralsDirectly) {
    const LemmatizationResult result =
        lemmatize_terms({"cats", "dogs", "systems", "models", "tokens"});

    EXPECT_EQ(result.lemmas,
              TokenList({"cat", "dog", "system", "model", "token"}));
    EXPECT_EQ(result.changed_token_count, 5U);
}

// Проверяется преобразование разных форм на -ies с восстановлением окончания -y.
TEST_F(LemmatizationTest, HandlesSeveralIesPluralFormsDirectly) {
    const LemmatizationResult result = lemmatize_terms(
        {"queries", "policies", "studies", "technologies"});

    EXPECT_EQ(result.lemmas,
              TokenList({"query", "policy", "study", "technology"}));
    EXPECT_EQ(result.changed_token_count, 4U);
}

// Проверяется обработка множественного числа на -sses, -shes, -ches, -xes, -zes и -oes.
TEST_F(LemmatizationTest, HandlesAllSupportedEsPluralEndings) {
    const LemmatizationResult result = lemmatize_terms(
        {"classes", "wishes", "churches", "boxes", "buzzes", "heroes"});

    EXPECT_EQ(result.lemmas,
              TokenList({"class", "wish", "church", "box", "buzz", "hero"}));
    EXPECT_EQ(result.changed_token_count, 6U);
}

// Проверяется, что каждое повторение одной формы множественного числа изменяется отдельно.
TEST_F(LemmatizationTest, LemmatizesEveryRepeatedPluralOccurrence) {
    const LemmatizationResult result =
        lemmatize_terms({"queries", "queries", "queries", "queries", "queries"});

    EXPECT_EQ(result.lemmas,
              TokenList({"query", "query", "query", "query", "query"}));
    EXPECT_EQ(result.changed_token_count, 5U);
}

// Проверяется чередование базовых и изменённых форм без потери порядка и повторов.
TEST_F(LemmatizationTest, PreservesOrderOfInterleavedBaseAndPluralForms) {
    const LemmatizationResult result = lemmatize_terms(
        {"query", "queries", "database", "databases", "query", "queries"});

    EXPECT_EQ(result.lemmas,
              TokenList({"query", "query", "database", "database", "query",
                         "query"}));
    EXPECT_EQ(result.changed_token_count, 3U);
}

// Проверяется повторная лемматизация нескольких форм на -ing в одном списке.
TEST_F(LemmatizationTest, LemmatizesEveryRepeatedIngForm) {
    const LemmatizationResult result = lemmatize_terms(
        {"running", "running", "making", "making", "retrieving", "retrieving"});

    EXPECT_EQ(result.lemmas,
              TokenList({"run", "run", "make", "make", "retrieve", "retrieve"}));
    EXPECT_EQ(result.changed_token_count, 6U);
}

// Проверяется повторная лемматизация нескольких форм на -ed в одном списке.
TEST_F(LemmatizationTest, LemmatizesEveryRepeatedEdForm) {
    const LemmatizationResult result = lemmatize_terms(
        {"stopped", "stopped", "retrieved", "retrieved", "indexed", "indexed"});

    EXPECT_EQ(result.lemmas,
              TokenList({"stop", "stop", "retrieve", "retrieve", "index", "index"}));
    EXPECT_EQ(result.changed_token_count, 6U);
}

// Проверяется удаление удвоенной согласной у разных форм на -ing и -ed.
TEST_F(LemmatizationTest, RemovesDoubledConsonantsFromVerbForms) {
    const LemmatizationResult result =
        lemmatize_terms({"running", "stopping", "stopped", "abutting", "abutted"});

    EXPECT_EQ(result.lemmas,
              TokenList({"run", "stop", "stop", "abut", "abut"}));
    EXPECT_EQ(result.changed_token_count, 5U);
}

// Проверяется восстановление конечной буквы e у разных форм глаголов.
TEST_F(LemmatizationTest, RestoresSilentEForVerbForms) {
    const LemmatizationResult result = lemmatize_terms(
        {"making", "creating", "retrieving", "normalized", "retrieved"});

    EXPECT_EQ(result.lemmas,
              TokenList({"make", "create", "retrieve", "normalize", "retrieve"}));
    EXPECT_EQ(result.changed_token_count, 5U);
}

// Проверяется несколько семейств неправильных существительных из встроенной таблицы.
TEST_F(LemmatizationTest, HandlesSeveralIrregularNounFamilies) {
    const LemmatizationResult result = lemmatize_terms(
        {"children", "men", "women", "mice", "feet", "teeth", "indices"});

    EXPECT_EQ(result.lemmas,
              TokenList({"child", "man", "woman", "mouse", "foot", "tooth",
                         "index"}));
    EXPECT_EQ(result.changed_token_count, 7U);
}

// Проверяется несколько семейств неправильных глаголов из встроенной таблицы.
TEST_F(LemmatizationTest, HandlesSeveralIrregularVerbFamilies) {
    const LemmatizationResult result = lemmatize_terms(
        {"went", "gone", "ran", "saw", "had", "has", "does"});

    EXPECT_EQ(result.lemmas,
              TokenList({"go", "go", "run", "see", "have", "have", "do"}));
    EXPECT_EQ(result.changed_token_count, 7U);
}

// Проверяется сведение всех поддерживаемых форм глагола be, включая am.
TEST_F(LemmatizationTest, HandlesEverySupportedFormOfVerbBeDirectly) {
    const LemmatizationResult result =
        lemmatize_terms({"be", "am", "is", "are", "was", "were", "been"});

    EXPECT_EQ(result.lemmas,
              TokenList({"be", "be", "be", "be", "be", "be", "be"}));
    EXPECT_EQ(result.changed_token_count, 6U);
}

// Проверяется обработка форм use, used и using без ошибочного усечения основы.
TEST_F(LemmatizationTest, CombinesBasePastAndContinuousFormsOfUse) {
    const LemmatizationResult result =
        lemmatize_terms({"use", "used", "using", "used", "use"});

    EXPECT_EQ(result.lemmas, TokenList({"use", "use", "use", "use", "use"}));
    EXPECT_EQ(result.changed_token_count, 3U);
}

// Проверяется техническое разрешение неоднозначной формы axes как осей координат.
TEST_F(LemmatizationTest, UsesTechnicalResolutionForAmbiguousAxesForm) {
    const LemmatizationResult result =
        lemmatize_terms({"axis", "axes", "axis", "axes"});

    EXPECT_EQ(result.lemmas, TokenList({"axis", "axis", "axis", "axis"}));
    EXPECT_EQ(result.changed_token_count, 2U);
}

// Проверяется применение словаря к формам, которые нельзя корректно обработать суффиксами.
TEST_F(LemmatizationTest, UsesDictionaryBeforeGenericSuffixRules) {
    const LemmatizationResult result =
        lemmatize_terms({"accompanied", "amplified", "applied"});

    EXPECT_EQ(result.lemmas, TokenList({"accompany", "amplify", "apply"}));
    EXPECT_EQ(result.changed_token_count, 3U);
}

// Проверяется выбор существующей словарной основы вместо обрезанных cod, comput и stor.
TEST_F(LemmatizationTest, ValidatesIngCandidatesAgainstLemmaVocabulary) {
    const LemmatizationResult result = lemmatize_terms(
        {"coding", "computing", "storing", "writing", "moving", "hoping",
         "singing", "filling"});

    EXPECT_EQ(result.lemmas,
              TokenList({"code", "compute", "store", "write", "move", "hope",
                         "sing", "fill"}));
    EXPECT_EQ(result.changed_token_count, 8U);
}

// Проверяется выбор существующей словарной основы для форм прошедшего времени.
TEST_F(LemmatizationTest, ValidatesEdCandidatesAgainstLemmaVocabulary) {
    const LemmatizationResult result = lemmatize_terms(
        {"coded", "computed", "stored", "moved", "hoped", "filled"});

    EXPECT_EQ(result.lemmas,
              TokenList({"code", "compute", "store", "move", "hope", "fill"}));
    EXPECT_EQ(result.changed_token_count, 6U);
}

// Проверяется отсутствие несуществующих обрезанных основ у технических глаголов.
TEST_F(LemmatizationTest, DoesNotProduceTruncatedTechnicalVerbStems) {
    const LemmatizationResult result =
        lemmatize_terms({"coding", "computing", "stored", "retrieving"});

    EXPECT_EQ(result.lemmas,
              TokenList({"code", "compute", "store", "retrieve"}));
    EXPECT_NE(result.lemmas[0], "cod");
    EXPECT_NE(result.lemmas[1], "comput");
    EXPECT_NE(result.lemmas[2], "stor");
}

// Проверяется сохранение неизвестных форм, для которых словарь не подтверждает основу.
TEST_F(LemmatizationTest, PreservesUnknownFormsWithoutValidLemmaCandidate) {
    const LemmatizationResult result =
        lemmatize_terms({"qzxying", "qzxyed", "qzxys"});

    EXPECT_EQ(result.lemmas, TokenList({"qzxying", "qzxyed", "qzxys"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется приоритет технических исключений над общими исключениями WordNet.
TEST_F(LemmatizationTest, AppliesTechnicalDomainOverridesBeforeWordNet) {
    const LemmatizationResult result = lemmatize_terms(
        {"data", "axes", "analytics", "physics", "statistics", "media"});

    EXPECT_EQ(result.lemmas,
              TokenList({"data", "axis", "analytics", "physics", "statistics",
                         "media"}));
    EXPECT_EQ(result.changed_token_count, 1U);
}

// Проверяется нормализация прикладных технических форм из файла исключений.
TEST_F(LemmatizationTest, NormalizesTechnicalFormsFromOverrides) {
    const LemmatizationResult result =
        lemmatize_terms({"apis", "statuses", "api", "status"});

    EXPECT_EQ(result.lemmas, TokenList({"api", "status", "api", "status"}));
    EXPECT_EQ(result.changed_token_count, 2U);
}

// Проверяется сохранение названий технических и научных областей во множественной форме.
TEST_F(LemmatizationTest, PreservesTechnicalDisciplineNames) {
    const LemmatizationResult result = lemmatize_terms(
        {"analytics", "economics", "electronics", "graphics", "informatics",
         "linguistics", "mathematics", "mechanics", "physics", "robotics",
         "semantics", "statistics"});

    EXPECT_EQ(result.lemmas,
              TokenList({"analytics", "economics", "electronics", "graphics",
                         "informatics", "linguistics", "mathematics", "mechanics",
                         "physics", "robotics", "semantics", "statistics"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется, что названия протоколов, библиотек и платформ не становятся обычными словами.
TEST_F(LemmatizationTest, PreservesProtocolLibraryAndPlatformNames) {
    const LemmatizationResult result = lemmatize_terms(
        {"https", "pandas", "windows", "kubernetes", "postgres", "devops"});

    EXPECT_EQ(result.lemmas,
              TokenList({"https", "pandas", "windows", "kubernetes", "postgres",
                         "devops"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется нормализация множественных форм распространённых технических сокращений.
TEST_F(LemmatizationTest, NormalizesPluralTechnicalAbbreviations) {
    const LemmatizationResult result = lemmatize_terms(
        {"ais", "apis", "clis", "cnns", "cpus", "dbs", "gans", "gpus",
         "guids", "ides", "jsons", "llms", "rnns", "sdks", "tpus", "uris",
         "urls", "uuids", "vms", "xmls"});

    EXPECT_EQ(result.lemmas,
              TokenList({"ai", "api", "cli", "cnn", "cpu", "db", "gan", "gpu",
                         "guid", "ide", "json", "llm", "rnn", "sdk", "tpu", "uri",
                         "url", "uuid", "vm", "xml"}));
    EXPECT_EQ(result.changed_token_count, 20U);
}

// Проверяется обработка технических форм, которые не покрываются общими правилами -s/-es.
TEST_F(LemmatizationTest, NormalizesExceptionalTechnicalPlurals) {
    const LemmatizationResult result =
        lemmatize_terms({"aliases", "caches", "regexes", "statuses"});

    EXPECT_EQ(result.lemmas,
              TokenList({"alias", "cache", "regex", "status"}));
    EXPECT_EQ(result.changed_token_count, 4U);
}

// Проверяется, что технические исключения не мешают обычным корректным множественным формам.
TEST_F(LemmatizationTest, StillNormalizesOrdinaryTechnicalPlurals) {
    const LemmatizationResult result = lemmatize_terms(
        {"systems", "models", "databases", "repositories", "dependencies",
         "libraries"});

    EXPECT_EQ(result.lemmas,
              TokenList({"system", "model", "database", "repository",
                         "dependency", "library"}));
    EXPECT_EQ(result.changed_token_count, 6U);
}

// Проверяется сохранение названий технологий, похожих на формы множественного числа.
TEST_F(LemmatizationTest, PreservesTechnicalNamesThatEndWithS) {
    const LemmatizationResult result =
        lemmatize_terms({"kubernetes", "devops", "windows"});

    EXPECT_EQ(result.lemmas, TokenList({"kubernetes", "devops", "windows"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется отдельная обработка каждого повторения улучшенной словарной формы.
TEST_F(LemmatizationTest, LemmatizesEveryRepeatedValidatedCandidate) {
    const LemmatizationResult result = lemmatize_terms(
        {"coding", "coding", "code", "computed", "computed", "compute"});

    EXPECT_EQ(result.lemmas,
              TokenList({"code", "code", "code", "compute", "compute",
                         "compute"}));
    EXPECT_EQ(result.changed_token_count, 4U);
}

// Проверяется несколько научных и латинских исключений из словаря WordNet.
TEST_F(LemmatizationTest, HandlesSeveralScientificDictionaryExceptions) {
    const LemmatizationResult result = lemmatize_terms(
        {"abaci", "alumnae", "oxen", "geese", "alumni", "appendices"});

    EXPECT_EQ(result.lemmas,
              TokenList({"abacus", "alumna", "ox", "goose", "alumnus",
                         "appendix"}));
    EXPECT_EQ(result.changed_token_count, 6U);
}

// Проверяется отдельная обработка каждого повторения словарного исключения.
TEST_F(LemmatizationTest, LemmatizesEveryRepeatedDictionaryException) {
    const LemmatizationResult result =
        lemmatize_terms({"abaci", "abaci", "abacus", "abaci", "abacus"});

    EXPECT_EQ(result.lemmas,
              TokenList({"abacus", "abacus", "abacus", "abacus", "abacus"}));
    EXPECT_EQ(result.changed_token_count, 3U);
}

// Проверяется сохранение каждого повторяющегося технического терма без изменений.
TEST_F(LemmatizationTest, PreservesEveryRepeatedTechnicalTermDirectly) {
    const LemmatizationResult result = lemmatize_terms(
        {"c++", "c++", "c#", "c#", ".net", ".net", "node.js", "node.js",
         "sha-256", "sha-256"});

    EXPECT_EQ(result.lemmas,
              TokenList({"c++", "c++", "c#", "c#", ".net", ".net", "node.js",
                         "node.js", "sha-256", "sha-256"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется сохранение версий, адресов, чисел и идентификаторов без искажения.
TEST_F(LemmatizationTest, PreservesNumericAndCompositeIdentifiers) {
    const LemmatizationResult result = lemmatize_terms(
        {"3.14", "192.168.1.1", "v2.1", "x86_64", "api.example.com"});

    EXPECT_EQ(result.lemmas,
              TokenList({"3.14", "192.168.1.1", "v2.1", "x86_64",
                         "api.example.com"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется сохранение кириллицы и иероглифов, для которых нет английских правил.
TEST_F(LemmatizationTest, PreservesNonLatinWords) {
    const LemmatizationResult result =
        lemmatize_terms({"поиск", "данные", "数据", "検索"});

    EXPECT_EQ(result.lemmas, TokenList({"поиск", "данные", "数据", "検索"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется, что защищённые окончания остаются неизменными при повторениях.
TEST_F(LemmatizationTest, PreservesRepeatedProtectedSingularEndings) {
    const LemmatizationResult result = lemmatize_terms(
        {"news", "news", "series", "species", "glass", "status", "analysis"});

    EXPECT_EQ(result.lemmas,
              TokenList({"news", "news", "series", "species", "glass", "status",
                         "analysis"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется сохранение коротких базовых слов, не подпадающих под правила окончаний.
TEST_F(LemmatizationTest, PreservesShortUninflectedWords) {
    const LemmatizationResult result =
        lemmatize_terms({"a", "an", "the", "to", "in", "on", "by"});

    EXPECT_EQ(result.lemmas, TokenList({"a", "an", "the", "to", "in", "on", "by"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется точный счётчик изменений в смеси базовых, изменённых и технических форм.
TEST_F(LemmatizationTest, CountsOnlyActuallyChangedTokensInMixedInput) {
    const LemmatizationResult result = lemmatize_terms(
        {"query", "queries", "c++", "children", "database", "databases",
         "node.js", "running", "run"});

    EXPECT_EQ(result.lemmas,
              TokenList({"query", "query", "c++", "child", "database",
                         "database", "node.js", "run", "run"}));
    EXPECT_EQ(result.changed_token_count, 4U);
}

// Проверяется соответствие числа выходных лемм числу входных токенов.
TEST_F(LemmatizationTest, ProducesExactlyOneLemmaForEveryInputToken) {
    const TokenList tokens = {"query", "queries", "children", "c++", "data",
                              "running", "status", "node.js"};
    const LemmatizationResult result = lemmatize_terms(tokens);

    EXPECT_EQ(result.lemmas.size(), tokens.size());
}

// Проверяется, что лемматизация не изменяет переданный массив исходных токенов.
TEST_F(LemmatizationTest, DoesNotModifyInputTokenList) {
    const TokenList tokens = {"queries", "children", "running", "c++"};
    const TokenList original = tokens;

    const LemmatizationResult result = lemmatize_terms(tokens);

    EXPECT_EQ(tokens, original);
    EXPECT_EQ(result.lemmas, TokenList({"query", "child", "run", "c++"}));
}

// Проверяется корректный пустой результат для пустого массива токенов.
TEST_F(LemmatizationTest, ReturnsEmptyResultForEmptyTokenList) {
    const TokenList tokens;
    const LemmatizationResult result = lemmatize_terms(tokens);

    EXPECT_TRUE(result.lemmas.empty());
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется сохранение пустого токена без обращения за пределы строки.
TEST_F(LemmatizationTest, SafelyPreservesEmptyToken) {
    const LemmatizationResult result = lemmatize_terms({"", "query", ""});

    EXPECT_EQ(result.lemmas, TokenList({"", "query", ""}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется работа встроенных и регулярных правил с пустым словарём исключений.
TEST_F(LemmatizationTest, AppliesCoreRulesWithoutExternalDictionary) {
    LemmaDictionary *empty_dictionary = lemma_dictionary_load(nullptr);
    ASSERT_NE(empty_dictionary, nullptr);
    const TokenList tokens = {"children", "queries", "running", "c++"};

    const LemmatizationResult result =
        lemmatize_tokens(&tokens, empty_dictionary);
    lemma_dictionary_destroy(empty_dictionary);

    EXPECT_EQ(result.lemmas, TokenList({"child", "query", "run", "c++"}));
    EXPECT_EQ(result.changed_token_count, 3U);
}

// Проверяется, что внешнее исключение не применяется при пустом словаре.
TEST_F(LemmatizationTest, LeavesDictionaryOnlyExceptionWithoutDictionary) {
    LemmaDictionary *empty_dictionary = lemma_dictionary_load("");
    ASSERT_NE(empty_dictionary, nullptr);
    const TokenList tokens = {"abaci", "data", "oxen"};

    const LemmatizationResult result =
        lemmatize_tokens(&tokens, empty_dictionary);
    lemma_dictionary_destroy(empty_dictionary);

    EXPECT_EQ(result.lemmas, TokenList({"abaci", "data", "oxen"}));
    EXPECT_EQ(result.changed_token_count, 0U);
}

// Проверяется большое число повторных форм без потери лемм и счётчика изменений.
TEST_F(LemmatizationTest, HandlesManyRepeatedInflectedForms) {
    TokenList tokens;
    for (size_t index = 0; index < 500; ++index) {
        tokens.emplace_back("queries");
        tokens.emplace_back("running");
        tokens.emplace_back("children");
        tokens.emplace_back("c++");
    }

    const LemmatizationResult result = lemmatize_terms(tokens);

    ASSERT_EQ(result.lemmas.size(), 2000U);
    EXPECT_EQ(result.changed_token_count, 1500U);
    for (size_t index = 0; index < result.lemmas.size(); index += 4) {
        EXPECT_EQ(result.lemmas[index], "query");
        EXPECT_EQ(result.lemmas[index + 1], "run");
        EXPECT_EQ(result.lemmas[index + 2], "child");
        EXPECT_EQ(result.lemmas[index + 3], "c++");
    }
}

}  // namespace
