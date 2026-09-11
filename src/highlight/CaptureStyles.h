#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <QColor>

#include "theme/Theme.h"

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

/// styleTable(), with every colour remapped from `theme` (font flags and key
/// names are theme-independent, so those come through unchanged).
std::vector<StyleDef> themedStyleTable(const Theme& theme);

/// Resolve a tree-sitter capture name (e.g. "keyword.control", "text.title")
/// to a style id, falling back through dotted prefixes. Returns StylePlain
/// when nothing matches.
int styleForCapture(std::string_view captureName);

/// The CSS class the preview wraps a token of `style` in, e.g.
/// "tok-string-escape". Empty for StylePlain / out-of-range ids.
std::string styleCssClass(int style);

} // namespace hungryeditor
