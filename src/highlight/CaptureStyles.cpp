#include "highlight/CaptureStyles.h"

#include <string>
#include <unordered_map>

namespace hungryeditor {

namespace {

// GitHub-light-ish token colours: the structural defaults styleTable()
// returns. themedStyleTable() below remaps the colours per the active theme;
// the key names and font flags set here are theme-independent.
StyleDef make(int id, const char* key, const char* hex, bool bold = false, bool italic = false,
              bool underline = false)
{
    return StyleDef{id, key, QColor(QString::fromLatin1(hex)), bold, italic, underline};
}

} // namespace

const std::vector<StyleDef>& styleTable()
{
    static const std::vector<StyleDef> table = {
        make(StylePlain, "plain", "#1e1e1e"),
        make(StyleKeyword, "keyword", "#cf222e"),
        make(StyleType, "type", "#953800"),
        make(StyleFunction, "function", "#6639ba"),
        make(StyleVariable, "variable", "#1e1e1e"),
        make(StyleProperty, "property", "#0550ae"),
        make(StyleString, "string", "#0a3069"),
        make(StyleStringEscape, "string.escape", "#0550ae", /*bold=*/true),
        make(StyleNumber, "number", "#0550ae"),
        make(StyleConstant, "constant", "#0550ae"),
        make(StyleComment, "comment", "#6e7781", /*bold=*/false, /*italic=*/true),
        make(StyleOperator, "operator", "#cf222e"),
        make(StylePunctuation, "punctuation", "#57606a"),
        make(StyleHeading, "heading", "#0550ae", /*bold=*/true),
        make(StyleEmphasis, "emphasis", "#1e1e1e", /*bold=*/false, /*italic=*/true),
        make(StyleStrong, "strong", "#1e1e1e", /*bold=*/true),
        make(StyleLink, "link", "#0969da", /*bold=*/false, /*italic=*/false, /*underline=*/true),
        make(StyleCodeLiteral, "code", "#6e40c9"),
    };
    return table;
}

std::vector<StyleDef> themedStyleTable(const Theme& theme)
{
    std::vector<StyleDef> table = styleTable();
    for (StyleDef& def : table) {
        switch (def.id) {
        case StylePlain:
        case StyleVariable:
        case StyleEmphasis:
        case StyleStrong:
            def.foreground = theme.text;
            break;
        case StyleKeyword:
        case StyleOperator:
            def.foreground = theme.keyword;
            break;
        case StyleType:
            def.foreground = theme.type;
            break;
        case StyleFunction:
            def.foreground = theme.function;
            break;
        case StyleProperty:
        case StyleNumber:
        case StyleConstant:
        case StyleStringEscape:
        case StyleHeading:
            def.foreground = theme.heading;
            break;
        case StyleString:
            def.foreground = theme.string;
            break;
        case StyleComment:
            def.foreground = theme.comment;
            break;
        case StylePunctuation:
            def.foreground = theme.muted;
            break;
        case StyleLink:
            def.foreground = theme.link;
            break;
        case StyleCodeLiteral:
            def.foreground = theme.codeText;
            break;
        default:
            break;
        }
    }
    return table;
}

int styleForCapture(std::string_view captureName)
{
    static const std::unordered_map<std::string, int> map = {
        // Markdown block + inline grammars (nvim-treesitter capture names).
        {"text.title", StyleHeading},
        {"text.literal", StyleCodeLiteral},
        {"text.emphasis", StyleEmphasis},
        {"text.strong", StyleStrong},
        {"text.uri", StyleLink},
        {"text.reference", StyleLink},
        {"punctuation.special", StylePunctuation},
        {"punctuation.delimiter", StylePunctuation},
        {"punctuation.bracket", StylePunctuation},
        {"punctuation", StylePunctuation},
        {"string.escape", StyleStringEscape},
        // Code grammars (used by fenced-block injections in later commits).
        {"keyword", StyleKeyword},
        {"conditional", StyleKeyword},
        {"repeat", StyleKeyword},
        {"include", StyleKeyword},
        {"tag", StyleKeyword},
        {"type", StyleType},
        {"constructor", StyleType},
        {"function", StyleFunction},
        {"method", StyleFunction},
        {"variable", StyleVariable},
        {"parameter", StyleVariable},
        {"field", StyleProperty},
        {"property", StyleProperty},
        {"attribute", StyleProperty},
        {"string", StyleString},
        {"character", StyleString},
        {"escape", StyleStringEscape},
        {"number", StyleNumber},
        {"float", StyleNumber},
        {"boolean", StyleConstant},
        {"constant", StyleConstant},
        {"label", StyleConstant},
        {"comment", StyleComment},
        {"operator", StyleOperator},
        {"punctuation.markup", StylePunctuation},
    };

    std::string key(captureName);
    while (!key.empty()) {
        const auto it = map.find(key);
        if (it != map.end()) {
            return it->second;
        }
        const auto dot = key.rfind('.');
        if (dot == std::string::npos) {
            break;
        }
        key.erase(dot);
    }
    return StylePlain;
}

std::string styleCssClass(int style)
{
    const auto& table = styleTable();
    if (style <= StylePlain || style >= static_cast<int>(table.size())) {
        return {};
    }
    std::string name = "tok-";
    for (const char* c = table[static_cast<std::size_t>(style)].key; *c != '\0'; ++c) {
        name += (*c == '.') ? '-' : *c;
    }
    return name;
}

} // namespace hungryeditor
