#pragma once

#include <string_view>
#include <vector>

namespace hungryeditor {

/// One coloured run inside a fenced code block. `style` is a Style enum value
/// (see CaptureStyles.h); byte offsets are into the highlighted code.
struct CodeToken
{
    int start = 0;
    int length = 0;
    int style = 0;
};

/// Syntax-highlight `code` as `language` (a fenced-code info string or alias,
/// e.g. "rust", "rs", "c++"). The result tiles the whole input in byte order,
/// including StylePlain runs. An unknown or unparseable language yields an
/// empty vector. Runs one tree-sitter parse; predicates in the highlight
/// queries are not evaluated, matching the editor's behaviour.
std::vector<CodeToken> highlightCode(std::string_view language, std::string_view code);

} // namespace hungryeditor
