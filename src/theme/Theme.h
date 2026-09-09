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

    /// A `<style>` sheet for the preview: `:root` custom properties followed by
    /// base element rules built from them.
    QString previewCss() const;
};

} // namespace hungryeditor
