#pragma once

#include <string_view>

#include <tree_sitter/api.h>

namespace hungryeditor {

/// A language grammar together with the source of its highlights.scm query.
struct Grammar
{
    const TSLanguage* language = nullptr;
    std::string_view highlights;
};

/// Resolve a fenced-code info string — or an injection language name — to a
/// grammar. Matching is case-insensitive and covers the usual aliases
/// ("rs" → rust, "c++" → cpp, "py" → python, ...). Returns an empty Grammar
/// (null `language`) when nothing is registered for the name.
Grammar grammarForName(std::string_view name);

/// A compiled highlights query for `language`, built once and cached for the
/// life of the process. Grammars and their highlights.scm queries are
/// immutable global data — the same language means the same query, whether
/// it is colouring a fenced block in the live editor (HighlightWorker, off
/// the GUI thread) or in a rendered preview (CodeHighlighter, on it) — so one
/// shared, thread-safe cache replaces what were previously two separate
/// compiles (and, for the preview path, a fresh recompile on every call).
/// `language` should be one grammarForName() already resolved; `highlights`
/// its matching Grammar::highlights. A failed compile is cached too, as
/// nullptr, so a broken query is never retried. The returned pointer is
/// owned by the cache — never ts_query_delete() it.
TSQuery* cachedHighlightsQuery(const TSLanguage* language, std::string_view highlights);

} // namespace hungryeditor
