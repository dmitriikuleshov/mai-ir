#include <gtest/gtest.h>

#include "linguistics/processing/html_extractor.hpp"

#include <string>

namespace {

// Проверяется выделение статьи Wikipedia без меню и навигационного блока.
TEST(HtmlExtractionTest, KeepsArticleAndRemovesNavigationFromWikipedia) {
    const std::string text = extract_text(
        "<html><nav>menu</nav><div id='mw-content-text'>"
        "<p>Useful article</p><div class='navbox'>noise</div>"
        "</div></html>",
        "wikipedia");

    EXPECT_NE(text.find("Useful article"), std::string::npos);
    EXPECT_EQ(text.find("noise"), std::string::npos);
    EXPECT_EQ(text.find("menu"), std::string::npos);
}

// Проверяется выделение текста arXiv без списка литературы.
TEST(HtmlExtractionTest, KeepsPaperAndRemovesBibliographyFromArxiv) {
    const std::string text = extract_text(
        "<article class='ltx_document'><p>Paper text</p>"
        "<section class='ltx_bibliography'>references</section></article>",
        "arxiv");

    EXPECT_NE(text.find("Paper text"), std::string::npos);
    EXPECT_EQ(text.find("references"), std::string::npos);
}

// Проверяется выделение документации Microsoft Learn без блока обратной связи.
TEST(HtmlExtractionTest, KeepsDocumentationAndRemovesFeedbackFromMicrosoftLearn) {
    const std::string text = extract_text(
        "<main><p>Documentation</p>"
        "<div class='feedback-section'>feedback</div></main>",
        "microsoft_learn");

    EXPECT_NE(text.find("Documentation"), std::string::npos);
    EXPECT_EQ(text.find("feedback"), std::string::npos);
}

// Проверяется замена HTML-сущностей и Unicode-пробелов обычным пробелом.
TEST(HtmlExtractionTest, NormalizesHtmlAndUnicodeSpaces) {
    const std::string text = extract_text(
        "<main><p>alpha&nbsp;beta\xE2\x80\xAFgamma</p></main>",
        "microsoft_learn");

    EXPECT_EQ(text, "alpha beta gamma");
}

}  // namespace
