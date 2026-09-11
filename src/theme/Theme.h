#pragma once

#include <QColor>
#include <QString>

namespace hungryeditor {

/// The colours that drive both the preview pane and the editor's own chrome
/// and syntax highlighting, so the two panes and every bundled or
/// user-supplied theme stay in sync.
struct Theme
{
    QColor background;     ///< page background; also the editor's background
    QColor text;           ///< body text; also the editor's foreground and caret
    QColor muted;          ///< de-emphasised text (rules, list markers); also line numbers
    QColor heading;        ///< h1-h6; also several "accent" syntax tokens (see codeTokenCss())
    QColor link;           ///< anchors
    QColor codeText;       ///< inline and block code text
    QColor codeBackground; ///< inline and block code background; also the line-number margin
    QColor border;         ///< blockquote bar, table cell borders
    QColor error;          ///< failed mermaid / katex render surfaces; also brace-match errors

    // Syntax tokens with no equivalent above (the rest reuse a field already
    // listed, matching what the built-in palettes' hex values coincided on).
    QColor keyword;  ///< keywords and operators
    QColor type;     ///< type and constructor names
    QColor function; ///< function and method names
    QColor string;   ///< string and character literals
    QColor comment;  ///< comments

    // Editor-only chrome with no preview equivalent.
    QColor currentLine; ///< subtle highlight behind the caret's line
    QColor selection;   ///< selection background
    QColor findMatch;   ///< find-bar match outline
    QColor braceMatch;  ///< matching-brace highlight background

    /// One of the bundled preview palettes.
    enum class Builtin
    {
        Light,
        Dark,
        HighContrast,
        Sepia,
    };

    /// The default light theme. Its hex values line up with the editor palette.
    static Theme builtin();

    /// The palette for `id`.
    static Theme forBuiltin(Builtin id);

    /// A human-readable label for `id`, e.g. "High Contrast".
    static QString builtinName(Builtin id);

    /// A stable machine key for `id` (for persistence), e.g. "high-contrast".
    static QString builtinKey(Builtin id);

    /// The id whose builtinKey() is `key`, or `fallback` when it matches none.
    static Builtin builtinFromKey(const QString& key, Builtin fallback = Builtin::Light);

    /// A `<style>` sheet for the preview: `:root` custom properties, base
    /// element rules, and the fenced-code token classes.
    QString previewCss() const;

    /// Just the `.tok-*` rules for highlighted fenced code, taken from the
    /// editor's token palette so the two panes match.
    QString codeTokenCss() const;
};

} // namespace hungryeditor
