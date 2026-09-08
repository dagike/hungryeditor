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

} // namespace hungryeditor
