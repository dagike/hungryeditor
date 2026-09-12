#include "highlight/CodeHighlighter.h"

#include <cstdint>
#include <string>

#include <tree_sitter/api.h>

#include "highlight/CaptureStyles.h"
#include "highlight/GrammarRegistry.h"
#include "highlight/QueryPredicates.h"
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

    // Shared with HighlightWorker's injection highlighting: the same
    // language means the same compiled query, so one process-lifetime cache
    // replaces what used to be a fresh ts_query_new() per fenced block.
    TSQuery* query = cachedHighlightsQuery(grammar.language, grammar.highlights);
    if (query == nullptr) {
        return tokens;
    }

    std::vector<int> byteStyle(code.size(), StylePlain);

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, engine.rootNode());
    // Matched (not flattened via next_capture) so a #match?/#eq?/#any-of?
    // predicate can see every capture of its own match — see
    // QueryPredicates.h.
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        if (!predicates::matchesPredicates(query, match, code)) {
            continue;
        }
        for (uint16_t i = 0; i < match.capture_count; ++i) {
            const TSQueryCapture& capture = match.captures[i];
            uint32_t nameLen = 0;
            const char* name = ts_query_capture_name_for_id(query, capture.index, &nameLen);
            const int style = styleForCapture(std::string_view(name, nameLen));
            if (style == StylePlain) {
                continue;
            }
            const uint32_t start = ts_node_start_byte(capture.node);
            const uint32_t end = ts_node_end_byte(capture.node);
            for (uint32_t j = start; j < end && j < byteStyle.size(); ++j) {
                byteStyle[j] = style;
            }
        }
    }
    ts_query_cursor_delete(cursor);
    // query is owned by the cache (see cachedHighlightsQuery()) — not deleted.

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
