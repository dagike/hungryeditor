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
/// empty vector. Runs one tree-sitter parse; highlight-query predicates
/// (#match?, #eq?, #any-of?, ...) are evaluated the same way as in the live
/// editor — see QueryPredicates.h — so a fenced block highlights identically
/// in the preview and while it's still being edited.
std::vector<CodeToken> highlightCode(std::string_view language, std::string_view code);

} // namespace hungryeditor
