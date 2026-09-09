#include "highlight/CodeHighlighter.h"

#include <cstdint>
#include <string>

#include <tree_sitter/api.h>

#include "highlight/CaptureStyles.h"
#include "highlight/GrammarRegistry.h"
#include "highlight/TreeSitterEngine.h"

namespace hungryeditor {

std::vector<CodeToken> highlightCode(std::string_view language, std::string_view code)
{
    std::vector<CodeToken> tokens;
    if (code.empty()) {
        return tokens;
    }

    const Grammar grammar = grammarForName(language);
    if (grammar.language == nullptr) {
        return tokens;
    }

    TreeSitterEngine engine;
    engine.setLanguage(grammar.language);
    engine.setText(std::string(code));
    if (!engine.hasTree()) {
        return tokens;
    }

    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    TSQuery* query =
        ts_query_new(grammar.language, grammar.highlights.data(),
                     static_cast<uint32_t>(grammar.highlights.size()), &errorOffset, &errorType);
    if (query == nullptr) {
        return tokens;
    }

    std::vector<int> byteStyle(code.size(), StylePlain);

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, engine.rootNode());
    TSQueryMatch match;
    uint32_t captureIndex = 0;
    while (ts_query_cursor_next_capture(cursor, &match, &captureIndex)) {
        const TSQueryCapture& capture = match.captures[captureIndex];
        uint32_t nameLen = 0;
        const char* name = ts_query_capture_name_for_id(query, capture.index, &nameLen);
        const int style = styleForCapture(std::string_view(name, nameLen));
        if (style == StylePlain) {
            continue;
        }
        const uint32_t start = ts_node_start_byte(capture.node);
        const uint32_t end = ts_node_end_byte(capture.node);
        for (uint32_t i = start; i < end && i < byteStyle.size(); ++i) {
            byteStyle[i] = style;
        }
    }
    ts_query_cursor_delete(cursor);
    ts_query_delete(query);

    for (std::size_t i = 0; i < byteStyle.size();) {
        const int style = byteStyle[i];
        std::size_t j = i + 1;
        while (j < byteStyle.size() && byteStyle[j] == style) {
            ++j;
        }
        tokens.push_back(CodeToken{static_cast<int>(i), static_cast<int>(j - i), style});
        i = j;
    }
    return tokens;
}

} // namespace hungryeditor
