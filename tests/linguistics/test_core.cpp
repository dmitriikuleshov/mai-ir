#define LINGUISTICS_TESTING
#include "../../src/linguistics/main.cpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int failures = 0;

static void expect(bool condition, const char *name) {
    if (condition) {
        std::printf("PASS | %s\n", name);
    } else {
        std::printf("FAIL | %s\n", name);
        ++failures;
    }
}

static uint64_t frequency(const FrequencyArray *items, const char *term) {
    for (size_t index = 0; index < items->size; ++index) {
        if (std::strcmp(items->items[index].term, term) == 0) {
            return items->items[index].count;
        }
    }
    return 0;
}

static bool file_contains(const char *path, const char *needle) {
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) return false;
    char line[4096];
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        if (std::strstr(line, needle) != nullptr) {
            std::fclose(file);
            return true;
        }
    }
    std::fclose(file);
    return false;
}

static long file_line(const char *path, const char *needle) {
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) return -1;
    char line[4096];
    long number = 0;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        ++number;
        if (std::strstr(line, needle) != nullptr) {
            std::fclose(file);
            return number;
        }
    }
    std::fclose(file);
    return -1;
}

static void test_tokenization() {
    LemmaDictionary *dictionary =
        lemma_dictionary_load("resources/wordnet_exceptions.tsv");
    const char *technical_text =
        "C++, C#, .NET, Node.js, state-of-the-art, version 3.14 and model's output.";
    LinguisticResult technical = linguistics_process(
        technical_text, std::strlen(technical_text), 100, dictionary);
    expect(frequency(&technical.tokens, "c++") == 1,
           "технический токен C++");
    expect(frequency(&technical.tokens, ".net") == 1,
           "технический токен .NET");
    expect(frequency(&technical.tokens, "state-of-the-art") == 1,
           "составной токен с дефисами");
    expect(frequency(&technical.tokens, "model") == 1,
           "удаление английского притяжательного окончания");
    linguistics_result_destroy(&technical);

    const char *unicode_text = "State‑of‑the‑art model’s";
    LinguisticResult unicode = linguistics_process(
        unicode_text, std::strlen(unicode_text), 100, dictionary);
    expect(frequency(&unicode.tokens, "state-of-the-art") == 1 &&
               frequency(&unicode.tokens, "model") == 1,
           "нормализация Unicode-дефисов и апострофов");
    linguistics_result_destroy(&unicode);

    char long_text[110];
    std::memset(long_text, 'x', sizeof(long_text));
    std::memcpy(long_text, "normal ", 7);
    long_text[109] = '\0';
    LinguisticResult limited = linguistics_process(
        long_text, std::strlen(long_text), 100, dictionary);
    expect(limited.token_count == 1 && frequency(&limited.tokens, "normal") == 1,
           "отбрасывание токена длиннее ограничения");
    linguistics_result_destroy(&limited);

    lemma_dictionary_destroy(dictionary);
}

static void test_morphology() {
    LemmaDictionary *dictionary =
        lemma_dictionary_load("resources/wordnet_exceptions.tsv");
    const char *morphology_text =
        "queries query queries retrieved abaci children";
    LinguisticResult morphology = linguistics_process(
        morphology_text, std::strlen(morphology_text), 100, dictionary);
    expect(frequency(&morphology.lemmas, "query") == 3,
           "лемматизация queries");
    expect(frequency(&morphology.lemmas, "retrieve") == 1,
           "лемматизация retrieved");
    expect(frequency(&morphology.lemmas, "abacus") == 1,
           "исключение WordNet abaci");
    expect(frequency(&morphology.lemmas, "child") == 1,
           "нерегулярная форма children");
    linguistics_result_destroy(&morphology);

    lemma_dictionary_destroy(dictionary);
}

static void test_extraction() {
    char *wikipedia = extract_text(
        "<html><nav>menu</nav><div id='mw-content-text'><p>Useful article</p>"
        "<div class='navbox'>noise</div></div></html>",
        "wikipedia");
    expect(std::strstr(wikipedia, "Useful article") != nullptr &&
               std::strstr(wikipedia, "noise") == nullptr &&
               std::strstr(wikipedia, "menu") == nullptr,
           "выделение текста Wikipedia");
    std::free(wikipedia);

    char *arxiv = extract_text(
        "<article class='ltx_document'><p>Paper text</p>"
        "<section class='ltx_bibliography'>references</section></article>",
        "arxiv");
    expect(std::strstr(arxiv, "Paper text") != nullptr &&
               std::strstr(arxiv, "references") == nullptr,
           "выделение текста arXiv");
    std::free(arxiv);

    char *learn = extract_text(
        "<main><p>Documentation</p><div class='feedback-section'>feedback</div></main>",
        "microsoft_learn");
    expect(std::strstr(learn, "Documentation") != nullptr &&
               std::strstr(learn, "feedback") == nullptr,
           "выделение текста Microsoft Learn");
    std::free(learn);

    char *unicode_spaces = extract_text(
        "<main><p>alpha&nbsp;beta\xE2\x80\xAFgamma</p></main>",
        "microsoft_learn");
    expect(std::strcmp(unicode_spaces, "alpha beta gamma") == 0,
           "Unicode-пробелы сохраняют корректный UTF-8");
    std::free(unicode_spaces);
}

static void test_config() {
    const Config config = load_config("config.yaml");
    expect(config.minimum_words == 200, "чтение YAML-конфигурации");
    expect(config.parser_version == 7, "версия C++-обработчика");

    char absolute_path[4096];
    const char *resolved = realpath("config.yaml", absolute_path);
    expect(resolved != nullptr, "абсолютный путь к конфигурации");
    if (resolved != nullptr) {
        const Config absolute_config = load_config(absolute_path);
        expect(absolute_config.lemma_dictionary[0] == '/' &&
                   absolute_config.output_directory[0] == '/',
               "пути разрешаются относительно config.yaml");
    }
}

static void test_mandelbrot() {
    const uint64_t sample[] = {1000, 520, 340, 250, 205, 170};
    const MandelbrotFit fit = fit_mandelbrot(sample, 6, 6);
    expect(fit.c > 0.0 && fit.b >= 0.0 && fit.s > 0.0 && fit.rmse >= 0.0,
           "аппроксимация Мандельброта");
}

static void test_no_stl() {
    const char *forbidden[] = {"<vector>", "<map>", "<unordered_map>",
                               "<string>", "<algorithm>"};
    bool no_stl = true;
    for (const char *header : forbidden) {
        no_stl = no_stl && !file_contains("src/linguistics/core.cpp", header) &&
                 !file_contains("src/linguistics/core.hpp", header) &&
                 !file_contains("src/linguistics/main.cpp", header);
    }
    expect(no_stl, "отсутствие STL-контейнеров в лингвистическом модуле");
}

static void test_recovery_protocol() {
    const long terms = file_line(
        "src/linguistics/main.cpp",
        "execute_bulk(terms_bulk, \"запись термов документов\")");
    const long processed = file_line(
        "src/linguistics/main.cpp",
        "execute_bulk(processed_bulk, \"запись обработанных документов\")");
    expect(terms > 0 && processed > terms,
           "обработанный документ записывается после document_terms");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr,
                     "usage: %s tokenization|morphology|extraction|config|"
                     "mandelbrot|no_stl|recovery\n",
                     argv[0]);
        return EXIT_FAILURE;
    }
    if (std::strcmp(argv[1], "tokenization") == 0) test_tokenization();
    else if (std::strcmp(argv[1], "morphology") == 0) test_morphology();
    else if (std::strcmp(argv[1], "extraction") == 0) test_extraction();
    else if (std::strcmp(argv[1], "config") == 0) test_config();
    else if (std::strcmp(argv[1], "mandelbrot") == 0) test_mandelbrot();
    else if (std::strcmp(argv[1], "no_stl") == 0) test_no_stl();
    else if (std::strcmp(argv[1], "recovery") == 0) test_recovery_protocol();
    else {
        std::fprintf(stderr, "unknown test: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    if (failures != 0) {
        std::printf("FAILED: %d\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("TEST PASSED | %s\n", argv[1]);
    return EXIT_SUCCESS;
}
