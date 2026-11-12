#include "linguistics/processing/html_extractor.hpp"

#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void remove_xpath_nodes(xmlXPathContextPtr context, const char *xpath) {
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

xmlNodePtr first_xpath_node(xmlXPathContextPtr context, const char *xpath) {
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

std::string normalize_whitespace(const char *input) {
    std::string output;
    output.reserve(std::strlen(input));
    bool pending_space = false;
    const unsigned char *cursor =
        reinterpret_cast<const unsigned char *>(input);
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
            pending_space = !output.empty();
            cursor += width;
            continue;
        }
        if (pending_space) {
            output.push_back(' ');
            pending_space = false;
        }
        output.append(reinterpret_cast<const char *>(cursor), width);
        cursor += width;
    }
    return output;
}

}  // namespace

std::string extract_text(const char *raw_html, const char *source) {
    htmlDocPtr document = htmlReadMemory(
        raw_html, static_cast<int>(std::strlen(raw_html)), nullptr, "UTF-8",
        HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING |
            HTML_PARSE_NONET | HTML_PARSE_HUGE);
    if (document == nullptr) return {};

    xmlXPathContextPtr context = xmlXPathNewContext(document);
    if (context == nullptr) {
        xmlFreeDoc(document);
        std::fprintf(stderr,
                     "linguistics: не удалось создать XPath-контекст\n");
        std::exit(EXIT_FAILURE);
    }

    const char *root_xpath = "//main";
    if (std::strcmp(source, "wikipedia") == 0) {
        root_xpath = "//*[@id='mw-content-text']";
    } else if (std::strcmp(source, "arxiv") == 0) {
        root_xpath = "//article[contains(concat(' ', normalize-space(@class), ' '), ' ltx_document ')]";
    }
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
    std::string text = normalize_whitespace(
        content == nullptr ? "" : reinterpret_cast<const char *>(content));
    if (content != nullptr) xmlFree(content);
    xmlXPathFreeContext(context);
    xmlFreeDoc(document);
    return text;
}
