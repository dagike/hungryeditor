#pragma once

#include <QColor>
#include <QString>

namespace hungryeditor {

/// The colours that drive both panes. Today one built-in theme exists and only
/// the preview consumes it; Phase 8 adds theme files and points the editor's
/// styling here too, at which point the two can no longer drift.
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

    /// The default light theme. Its hex values line up with the editor palette.
    static Theme builtin();

    /// A `<style>` sheet for the preview: `:root` custom properties, base
    /// element rules, and the fenced-code token classes.
    QString previewCss() const;

    /// Just the `.tok-*` rules for highlighted fenced code, taken from the
    /// editor's token palette so the two panes match.
    QString codeTokenCss() const;
};

} // namespace hungryeditor
