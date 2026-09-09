#pragma once

#include <QString>

namespace hungryeditor {

/// Converts Markdown source into an HTML fragment for the preview pane.
///
/// Every block-level element carries a `data-src-line="N"` attribute — the
/// zero-based source line it originates from — so the preview can keep its
/// scroll position aligned with the editor and move the caret when a heading
/// is clicked. Parsing follows CommonMark; GFM extensions arrive in a later
/// phase.
class Md4cRenderer
{
public:
    /// `markdown` is UTF-8 source. The result is a fragment with no
    /// `<html>`/`<head>`/`<body>` wrapper, ready to drop into a template.
    QString toHtml(const QString& markdown) const;
};

} // namespace hungryeditor
