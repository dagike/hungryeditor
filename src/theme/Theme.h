#pragma once

#include <QColor>
#include <QString>

namespace hungryeditor {

/// The colours that drive the preview pane. Four built-ins exist today; a
/// later Phase 8 commit adds user-supplied theme files. Unifying the editor's
/// own (still separately hard-coded) palette with this one remains future
/// work.
struct Theme
{
    QColor background;     ///< page background
    QColor text;           ///< body text
    QColor muted;          ///< de-emphasised text (rules, list markers)
    QColor heading;        ///< h1-h6
    QColor link;           ///< anchors
    QColor codeText;       ///< inline and block code text
    QColor codeBackground; ///< inline and block code background
    QColor border;         ///< blockquote bar, table cell borders
    QColor error;          ///< failed mermaid / katex render surfaces

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
