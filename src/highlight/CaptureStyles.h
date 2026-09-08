#pragma once

#include <string_view>
#include <vector>

#include <QColor>

namespace hungryeditor {

/// A Scintilla style used by the container-lexing highlighter. Style 0 is
/// plain text; semantic tokens use ids 1..N.
struct StyleDef
{
    int id = 0;
    const char* key = "";
    QColor foreground;
    bool bold = false;
    bool italic = false;
    bool underline = false;
};

/// Semantic style ids. Kept small and stable; injected code grammars in later
/// commits reuse the same set.
enum Style : int
{
    StylePlain = 0,
    StyleKeyword,
    StyleType,
    StyleFunction,
    StyleVariable,
    StyleProperty,
    StyleString,
    StyleStringEscape,
    StyleNumber,
    StyleConstant,
    StyleComment,
    StyleOperator,
    StylePunctuation,
    StyleHeading,
    StyleEmphasis,
    StyleStrong,
    StyleLink,
    StyleCodeLiteral,

    StyleCount
};

/// Every semantic style with its colour and font flags, in `id` order.
const std::vector<StyleDef>& styleTable();

/// Resolve a tree-sitter capture name (e.g. "keyword.control", "text.title")
/// to a style id, falling back through dotted prefixes. Returns StylePlain
/// when nothing matches.
int styleForCapture(std::string_view captureName);

} // namespace hungryeditor
