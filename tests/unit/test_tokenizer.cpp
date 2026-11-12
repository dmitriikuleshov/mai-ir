#include <gtest/gtest.h>

#include "linguistics/language/tokenizer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

TokenizationResult tokenize(std::string_view text,
                            size_t maximum_length = 100) {
    return tokenize_text(text.data(), text.size(), maximum_length);
}

void expect_tokens(std::string_view text,
                   std::initializer_list<const char *> expected,
                   size_t maximum_length = 100) {
    const TokenizationResult result = tokenize(text, maximum_length);
    std::vector<std::string> expected_tokens;
    expected_tokens.reserve(expected.size());
    for (const char *token : expected) expected_tokens.emplace_back(token);
    EXPECT_EQ(result.tokens, expected_tokens);
}

size_t occurrences(const TokenizationResult &result, std::string_view token) {
    size_t count = 0;
    for (const std::string &candidate : result.tokens) {
        if (candidate == token) ++count;
    }
    return count;
}

// Проверяется приведение ASCII-букв к нижнему регистру с сохранением повторов.
TEST(TokenizerTest, NormalizesAsciiCaseAndPreservesDuplicates) {
    expect_tokens("Database DATABASE DaTaBaSe",
                  {"database", "database", "database"});
}

// Проверяется удаление разных знаков пунктуации вокруг обычных слов.
TEST(TokenizerTest, RemovesSurroundingPunctuation) {
    expect_tokens("(database), [retrieval]; {index}! \"search\"?",
                  {"database", "retrieval", "index", "search"});
}

// Проверяется сохранение нескольких разных технических терминов в одном тексте.
TEST(TokenizerTest, PreservesTechnicalTermsAsSingleTokens) {
    expect_tokens("C++ C# .NET node.js snake_case sha-256 3.14 state-of-the-art",
                  {"c++", "c#", ".net", "node.js", "snake_case", "sha-256",
                   "3.14", "state-of-the-art"});
}

// Проверяется, что повторяющиеся технические термины не теряются и не склеиваются.
TEST(TokenizerTest, PreservesRepeatedTechnicalTermsInMixedText) {
    expect_tokens("C++ and c++; Node.js talks to NODE.JS; SHA-256 equals sha‑256.",
                  {"c++", "and", "c++", "node.js", "talks", "to", "node.js",
                   "sha-256", "equals", "sha-256"});
}

// Проверяется явное сохранение каждого из пяти повторных вхождений C++.
TEST(TokenizerTest, KeepsEveryRepeatedCppOccurrence) {
    const TokenizationResult result = tokenize("C++ c++ (C++) C++/C++");

    ASSERT_EQ(result.tokens.size(), 5U);
    EXPECT_EQ(occurrences(result, "c++"), 5U);
    EXPECT_EQ(result.token_characters, 15U);
}

// Проверяется повторная обработка нескольких технических термов в одной строке.
TEST(TokenizerTest, KeepsTwoOccurrencesOfEveryTechnicalTerm) {
    const TokenizationResult result =
        tokenize("C# c# .NET .net Node.js NODE.JS SHA-256 sha‑256");

    ASSERT_EQ(result.tokens.size(), 8U);
    EXPECT_EQ(occurrences(result, "c#"), 2U);
    EXPECT_EQ(occurrences(result, ".net"), 2U);
    EXPECT_EQ(occurrences(result, "node.js"), 2U);
    EXPECT_EQ(occurrences(result, "sha-256"), 2U);
}

// Проверяется подсчёт повторов при чередовании разных технических терминов.
TEST(TokenizerTest, KeepsInterleavedRepeatedTechnicalTerms) {
    const TokenizationResult result =
        tokenize("C++ node.js C++ C# node.js C++ C#");

    EXPECT_EQ(occurrences(result, "c++"), 3U);
    EXPECT_EQ(occurrences(result, "node.js"), 2U);
    EXPECT_EQ(occurrences(result, "c#"), 2U);
    EXPECT_EQ(result.tokens,
              std::vector<std::string>({"c++", "node.js", "c++", "c#",
                                        "node.js", "c++", "c#"}));
}

// Проверяется, что пунктуация между повторами C++ не приводит к их склеиванию.
TEST(TokenizerTest, SeparatesRepeatedCppTermsAroundPunctuation) {
    const TokenizationResult result = tokenize("C++,C++;C++|C++/C++");

    EXPECT_EQ(result.tokens.size(), 5U);
    EXPECT_EQ(occurrences(result, "c++"), 5U);
}

// Проверяется сохранение повторных технических термов на разных строках текста.
TEST(TokenizerTest, KeepsRepeatedTermsAcrossLineBreaks) {
    const TokenizationResult result =
        tokenize("C++\nNode.js\r\nC++\tNode.js\nC++");

    EXPECT_EQ(occurrences(result, "c++"), 3U);
    EXPECT_EQ(occurrences(result, "node.js"), 2U);
    EXPECT_EQ(result.tokens.size(), 5U);
}

// Проверяется, что разные варианты дефиса дают несколько одинаковых токенов.
TEST(TokenizerTest, KeepsRepeatedTermsAfterUnicodeDashNormalization) {
    const TokenizationResult result = tokenize(
        "state-of-the-art state‑of‑the‑art state—of—the—art");

    EXPECT_EQ(result.tokens.size(), 3U);
    EXPECT_EQ(occurrences(result, "state-of-the-art"), 3U);
}

// Проверяется, что притяжательные формы и обычная форма дают отдельные повторы токена.
TEST(TokenizerTest, KeepsRepeatedWordsAfterPossessiveNormalization) {
    const TokenizationResult result =
        tokenize("model's MODEL’S model model’s");

    EXPECT_EQ(result.tokens.size(), 4U);
    EXPECT_EQ(occurrences(result, "model"), 4U);
}

// Проверяется сохранение повторов на самой границе максимальной длины токена.
TEST(TokenizerTest, KeepsRepeatedTermsAtMaximumLengthBoundary) {
    const TokenizationResult result = tokenize("C++ c++ .NET .net", 3);

    EXPECT_EQ(result.tokens, std::vector<std::string>({"c++", "c++"}));
    EXPECT_EQ(occurrences(result, "c++"), 2U);
    EXPECT_EQ(occurrences(result, ".net"), 0U);
}

// Проверяется, что похожие обозначения C, C++, C# и CPP не смешиваются между собой.
TEST(TokenizerTest, DoesNotMergeSimilarTechnicalTerms) {
    const TokenizationResult result = tokenize("C C++ C# CPP C++ C# C");

    EXPECT_EQ(occurrences(result, "c"), 2U);
    EXPECT_EQ(occurrences(result, "c++"), 2U);
    EXPECT_EQ(occurrences(result, "c#"), 2U);
    EXPECT_EQ(occurrences(result, "cpp"), 1U);
    EXPECT_EQ(result.tokens.size(), 7U);
}

// Проверяется большое число чередующихся повторов без потери отдельных вхождений.
TEST(TokenizerTest, KeepsManyRepeatedTechnicalTerms) {
    std::string text;
    for (size_t index = 0; index < 100; ++index) {
        text += "C++ Node.js C# ";
    }

    const TokenizationResult result = tokenize(text);
    ASSERT_EQ(result.tokens.size(), 300U);
    EXPECT_EQ(occurrences(result, "c++"), 100U);
    EXPECT_EQ(occurrences(result, "node.js"), 100U);
    EXPECT_EQ(occurrences(result, "c#"), 100U);
}

// Проверяется нормализация типографских дефисов, апострофов и неразрывного пробела.
TEST(TokenizerTest, NormalizesUnicodeDashesApostrophesAndSpaces) {
    expect_tokens("model's model’s state‑of‑the‑art alpha beta",
                  {"model", "model", "state-of-the-art", "alpha", "beta"});
}

// Проверяется одинаковая нормализация пяти распространённых Unicode-дефисов.
TEST(TokenizerTest, NormalizesAllSupportedUnicodeDashes) {
    expect_tokens("a‐b a‑b a‒b a–b a—b",
                  {"a-b", "a-b", "a-b", "a-b", "a-b"});
}

// Проверяется разделение по табуляции, переводам строк и управляющим пробелам ASCII.
TEST(TokenizerTest, SplitsOnAsciiWhitespaceCharacters) {
    expect_tokens("alpha\tbeta\ngamma\rdelta\fepsilon\vzeta",
                  {"alpha", "beta", "gamma", "delta", "epsilon", "zeta"});
}

// Проверяется разделение по тонкому, узкому неразрывному и идеографическому пробелам.
TEST(TokenizerTest, SplitsOnDifferentUnicodeSpaces) {
    expect_tokens("alpha beta gamma　delta",
                  {"alpha", "beta", "gamma", "delta"});
}

// Проверяется отбрасывание слишком длинного токена с продолжением разбора текста.
TEST(TokenizerTest, DiscardsTooLongTokenAndContinuesParsing) {
    expect_tokens("normal abcdefghijk tail", {"normal", "tail"}, 10);
}

// Проверяется включение токена на границе длины и исключение следующего за границей.
TEST(TokenizerTest, AppliesMaximumLengthBoundaryExactly) {
    expect_tokens("abcdefghij abcdefghijk tail", {"abcdefghij", "tail"}, 10);
}

// Проверяется, что нулевое ограничение длины исключает все непустые токены.
TEST(TokenizerTest, RejectsAllTokensWhenMaximumLengthIsZero) {
    expect_tokens("a database 42", {}, 0);
}

// Проверяется, что в длину технического токена входят точки и знаки языка.
TEST(TokenizerTest, CountsTechnicalSeparatorsInMaximumLength) {
    expect_tokens("C++ .NET node.js", {"c++", ".net"}, 4);
}

// Проверяется измерение длины Unicode-токена в символах, а не в байтах UTF-8.
TEST(TokenizerTest, MeasuresUnicodeLengthInCodepoints) {
    const TokenizationResult accepted = tokenize("данные", 6);
    ASSERT_EQ(accepted.tokens, std::vector<std::string>({"данные"}));
    EXPECT_EQ(accepted.token_characters, 6U);

    const TokenizationResult rejected = tokenize("данные", 5);
    EXPECT_TRUE(rejected.tokens.empty());
    EXPECT_EQ(rejected.token_characters, 0U);
}

// Проверяется удаление английского притяжательного окончания в двух вариантах.
TEST(TokenizerTest, StripsAsciiAndUnicodePossessiveEndings) {
    expect_tokens("database's DATABASE'S model’s system’S",
                  {"database", "database", "model", "system"});
}

// Проверяется сохранение апострофов внутри сокращений и составных слов.
TEST(TokenizerTest, PreservesInternalApostrophesThatAreNotPossessives) {
    expect_tokens("don't we're rock'n'roll", {"don't", "we're", "rock'n'roll"});
}

// Проверяется преобразование полноширинных букв, цифр и технической пунктуации.
TEST(TokenizerTest, NormalizesFullwidthAsciiCharacters) {
    expect_tokens("ＤＡＴＡＢＡＳＥ，ＮＯＤＥ．ＪＳ Ｃ＋＋ １２３",
                  {"database", "node.js", "c++", "123"});
}

// Проверяется сохранение версий, IP-адресов, доменов и идентификаторов платформ.
TEST(TokenizerTest, PreservesVersionsAddressesDomainsAndIdentifiers) {
    expect_tokens("IPv4 v2.1.7 x86_64 api.example.com 192.168.1.1",
                  {"ipv4", "v2.1.7", "x86_64", "api.example.com",
                   "192.168.1.1"});
}

// Проверяется сохранение нескольких внутренних дефисов, точек и подчёркиваний.
TEST(TokenizerTest, PreservesSupportedInnerSeparators) {
    expect_tokens("mother-in-law package.module.name long_identifier_name",
                  {"mother-in-law", "package.module.name",
                   "long_identifier_name"});
}

// Проверяется удаление внутренних разделителей, если после них не продолжается слово.
TEST(TokenizerTest, DropsTrailingInnerSeparators) {
    expect_tokens("alpha. beta- gamma_ delta'", {"alpha", "beta", "gamma", "delta"});
}

// Проверяется поддержка начальной точки в названиях платформ и скрытых файлов.
TEST(TokenizerTest, PreservesLeadingDotBeforeWord) {
    expect_tokens(".NET .config ..hidden", {".net", ".config", ".hidden"});
}

// Проверяется, что символы подчёркивания, дефиса и решётки в начале не входят в токен.
TEST(TokenizerTest, IgnoresUnsupportedLeadingSeparators) {
    expect_tokens("_private -option #hashtag", {"private", "option", "hashtag"});
}

// Проверяется, что плюс и решётка сохраняются только как окончания C++-подобных термов.
TEST(TokenizerTest, HandlesPlusAndHashOnlyAsTechnicalSuffixes) {
    expect_tokens("C++ C# A+B C##", {"c++", "c#", "a", "b", "c#"});
}

// Проверяется разделение слов по нескольким внешним ASCII-разделителям подряд.
TEST(TokenizerTest, DoesNotJoinWordsAcrossAsciiSeparators) {
    expect_tokens("cloud/database;search|indexing&ranking",
                  {"cloud", "database", "search", "indexing", "ranking"});
}

// Проверяется разделение по стрелкам, математическим знакам и графическим символам.
TEST(TokenizerTest, DoesNotJoinWordsAcrossUnicodeSymbols) {
    expect_tokens("alpha→beta gamma≠delta epsilon✓zeta",
                  {"alpha", "beta", "gamma", "delta", "epsilon", "zeta"});
}

// Проверяется сохранение базового URL и адреса электронной почты целыми токенами.
TEST(TokenizerTest, PreservesUrlsAndEmailAddresses) {
    expect_tokens("https://example.com/docs user@example.com",
                  {"https://example.com/docs", "user@example.com"});
}

// Проверяется сохранение пространств имён C++ с одним и несколькими разделителями.
TEST(TokenizerTest, PreservesCppNamespaceNames) {
    expect_tokens("std::vector std::chrono::steady_clock boost::asio",
                  {"std::vector", "std::chrono::steady_clock", "boost::asio"});
}

// Проверяется отдельная обработка каждого повторения пространства имён C++.
TEST(TokenizerTest, KeepsRepeatedCppNamespaceNames) {
    const TokenizationResult result =
        tokenize("std::vector std::vector boost::asio std::vector");

    EXPECT_EQ(occurrences(result, "std::vector"), 3U);
    EXPECT_EQ(occurrences(result, "boost::asio"), 1U);
    EXPECT_EQ(result.tokens.size(), 4U);
}

// Проверяется сохранение длинных параметров командной строки и значения после '='.
TEST(TokenizerTest, PreservesLongCommandLineOptions) {
    expect_tokens("--output-dir --dry-run --threads=8",
                  {"--output-dir", "--dry-run", "--threads=8"});
}

// Проверяется сохранение повторяющихся параметров среди обычных слов программы.
TEST(TokenizerTest, KeepsRepeatedCommandLineOptionsInText) {
    const TokenizationResult result =
        tokenize("Run --verbose now; --verbose enables logs; use --verbose again.");

    EXPECT_EQ(occurrences(result, "--verbose"), 3U);
    EXPECT_EQ(occurrences(result, "run"), 1U);
    EXPECT_EQ(occurrences(result, "logs"), 1U);
}

// Проверяется сохранение абсолютных Unix-путей с каталогами, точками и дефисами.
TEST(TokenizerTest, PreservesAbsoluteUnixPaths) {
    expect_tokens("/usr/bin/python /etc/nginx/nginx.conf /home/user/my-file.txt",
                  {"/usr/bin/python", "/etc/nginx/nginx.conf",
                   "/home/user/my-file.txt"});
}

// Проверяется удаление внешней пунктуации вокруг путей без изменения самого пути.
TEST(TokenizerTest, StripsPunctuationAroundAbsolutePaths) {
    expect_tokens("(/usr/bin/python), [/etc/hosts]; \"/tmp/data.json\".",
                  {"/usr/bin/python", "/etc/hosts", "/tmp/data.json"});
}

// Проверяется сохранение scoped-пакетов npm и версии после второго символа '@'.
TEST(TokenizerTest, PreservesScopedPackageNames) {
    expect_tokens("@angular/core @types/node @openai/sdk@17.2",
                  {"@angular/core", "@types/node", "@openai/sdk@17.2"});
}

// Проверяется сохранение адресов с точками и многоуровневыми доменами.
TEST(TokenizerTest, PreservesDifferentEmailAddressForms) {
    expect_tokens("user@example.com first.last@sub.example.org",
                  {"user@example.com", "first.last@sub.example.org"});
}

// Проверяется сохранение URL с портом и несколькими сегментами пути.
TEST(TokenizerTest, PreservesUrlsWithPortsAndPaths) {
    expect_tokens("http://localhost:8080/api/v1 https://example.com/docs/search",
                  {"http://localhost:8080/api/v1",
                   "https://example.com/docs/search"});
}

// Проверяется сохранение параметров запроса, percent-encoding и фрагмента URL.
TEST(TokenizerTest, PreservesUrlQueryAndFragment) {
    expect_tokens("https://example.com/search?q=C%2B%2B&lang=en#section",
                  {"https://example.com/search?q=c%2b%2b&lang=en#section"});
}

// Проверяется совместная обработка разных технических конструкций в одном тексте.
TEST(TokenizerTest, PreservesMixedTechnicalConstructions) {
    expect_tokens(
        "Use std::vector with --output-dir=/tmp and visit "
        "https://example.com/docs or email dev@example.com for @openai/sdk.",
        {"use", "std::vector", "with", "--output-dir", "/tmp", "and", "visit",
         "https://example.com/docs", "or", "email", "dev@example.com", "for",
         "@openai/sdk"});
}

// Проверяется, что обычная косая черта в прозе не превращает два слова в путь.
TEST(TokenizerTest, DoesNotTreatOrdinarySlashSeparatedWordsAsAbsolutePath) {
    expect_tokens("client/server input/output cloud/database",
                  {"client", "server", "input", "output", "cloud", "database"});
}

// Проверяется учёт технических префиксов и разделителей в ограничении длины.
TEST(TokenizerTest, CountsTechnicalPrefixesAndSeparatorsInMaximumLength) {
    expect_tokens("--help std::vector /bin/sh", {"--help", "/bin/sh"}, 7);
}

// Проверяется выделение целых, дробных, отрицательных чисел и процентов.
TEST(TokenizerTest, TokenizesCommonNumericForms) {
    expect_tokens("42 3.1415 -273.15 +7 99% $100",
                  {"42", "3.1415", "273.15", "7", "99", "100"});
}

// Проверяется работа с кириллицей, иероглифами и смешанными системами письма.
TEST(TokenizerTest, PreservesNonLatinUnicodeWords) {
    expect_tokens("поиск данные 数据 検索", {"поиск", "данные", "数据", "検索"});
}

// Проверяется, что встроенный нулевой байт считается границей, а не концом буфера.
TEST(TokenizerTest, HandlesEmbeddedNullByteUsingExplicitTextSize) {
    const std::string text("alpha\0beta", 10);
    const TokenizationResult result = tokenize(text);
    EXPECT_EQ(result.tokens, std::vector<std::string>({"alpha", "beta"}));
}

// Проверяется декодирование корректных двух-, трёх- и четырёхбайтовых символов UTF-8.
TEST(TokenizerTest, DecodesValidMultibyteUtf8Characters) {
    expect_tokens("café поиск 𐍈", {"café", "поиск", "𐍈"});
}

// Проверяется отбрасывание ведущего байта с неправильным продолжающим байтом.
TEST(TokenizerTest, RejectsInvalidUtf8ContinuationByte) {
    std::string text = "alpha";
    text.push_back(static_cast<char>(0xC3));
    text.push_back('(');
    text += "beta";

    const TokenizationResult result = tokenize(text);
    EXPECT_EQ(result.tokens, std::vector<std::string>({"alpha", "beta"}));
}

// Проверяется безопасная обработка оборванной UTF-8-последовательности в конце текста.
TEST(TokenizerTest, RejectsTruncatedUtf8Sequence) {
    std::string text = "alpha";
    text.push_back(static_cast<char>(0xE2));
    text.push_back(static_cast<char>(0x82));

    const TokenizationResult result = tokenize(text);
    EXPECT_EQ(result.tokens, std::vector<std::string>({"alpha"}));
}

// Проверяется отклонение избыточной двухбайтовой кодировки ASCII-символа.
TEST(TokenizerTest, RejectsOverlongUtf8Encoding) {
    std::string text = "left";
    text.push_back(static_cast<char>(0xC0));
    text.push_back(static_cast<char>(0xAF));
    text += "right";

    const TokenizationResult result = tokenize(text);
    EXPECT_EQ(result.tokens, std::vector<std::string>({"left", "right"}));
}

// Проверяется отклонение UTF-8-последовательности из surrogate-диапазона Unicode.
TEST(TokenizerTest, RejectsUtf8EncodedSurrogateCodepoint) {
    std::string text = "left";
    text.push_back(static_cast<char>(0xED));
    text.push_back(static_cast<char>(0xA0));
    text.push_back(static_cast<char>(0x80));
    text += "right";

    const TokenizationResult result = tokenize(text);
    EXPECT_EQ(result.tokens, std::vector<std::string>({"left", "right"}));
}

// Проверяется отклонение code point, превышающего максимальное значение Unicode.
TEST(TokenizerTest, RejectsCodepointAboveUnicodeMaximum) {
    std::string text = "left";
    text.push_back(static_cast<char>(0xF4));
    text.push_back(static_cast<char>(0x90));
    text.push_back(static_cast<char>(0x80));
    text.push_back(static_cast<char>(0x80));
    text += "right";

    const TokenizationResult result = tokenize(text);
    EXPECT_EQ(result.tokens, std::vector<std::string>({"left", "right"}));
}

// Проверяется, что токенизатор не читает данные после переданного размера буфера.
TEST(TokenizerTest, RespectsExplicitTextSize) {
    constexpr char text[] = "alpha beta";
    const TokenizationResult result = tokenize_text(text, 5, 100);
    EXPECT_EQ(result.tokens, std::vector<std::string>({"alpha"}));
}

// Проверяется безопасная обработка нулевого указателя при пустом входном буфере.
TEST(TokenizerTest, AcceptsNullPointerForEmptyInput) {
    const TokenizationResult result = tokenize_text(nullptr, 0, 100);
    EXPECT_TRUE(result.tokens.empty());
    EXPECT_EQ(result.token_characters, 0U);
}

// Проверяется пустой результат для пустой строки, пробелов и одной пунктуации.
TEST(TokenizerTest, ReturnsEmptyResultWhenInputContainsNoWords) {
    EXPECT_TRUE(tokenize("").tokens.empty());
    EXPECT_TRUE(tokenize(" \t\n ").tokens.empty());
    EXPECT_TRUE(tokenize("... ,,, --- !!! ???").tokens.empty());
}

// Проверяется сохранение исходного порядка и всех вхождений токенов.
TEST(TokenizerTest, PreservesOriginalOrderAndEveryOccurrence) {
    expect_tokens("zeta alpha beta alpha", {"zeta", "alpha", "beta", "alpha"});
}

// Проверяется подсчёт символов после нормализации и удаления притяжательного окончания.
TEST(TokenizerTest, CountsCharactersOfAcceptedNormalizedTokens) {
    const TokenizationResult result = tokenize("C++ model's sha‑256");

    EXPECT_EQ(result.tokens,
              std::vector<std::string>({"c++", "model", "sha-256"}));
    EXPECT_EQ(result.token_characters, 15U);
}

// Проверяется, что число символов учитывает каждое повторное вхождение токена.
TEST(TokenizerTest, CountsCharactersOfDuplicateTokensSeparately) {
    const TokenizationResult result = tokenize("one one one");

    EXPECT_EQ(result.tokens.size(), 3U);
    EXPECT_EQ(result.token_characters, 9U);
}

// Проверяется стабильная обработка длинного текста с большим числом повторов.
TEST(TokenizerTest, HandlesLargeInputWithoutLosingTokens) {
    std::string text;
    for (size_t index = 0; index < 2000; ++index) text += "Node.js ";

    const TokenizationResult result = tokenize(text);
    ASSERT_EQ(result.tokens.size(), 2000U);
    EXPECT_EQ(result.tokens.front(), "node.js");
    EXPECT_EQ(result.tokens.back(), "node.js");
    EXPECT_EQ(result.token_characters, 14000U);
}

}  // namespace
