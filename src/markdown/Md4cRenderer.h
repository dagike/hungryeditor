#pragma once

#include <QString>

namespace hungryeditor {

/// Converts Markdown source into an HTML fragment for the preview pane.
///
/// Every block-level element carries a `data-src-line="N"` attribute — the
/// zero-based source line it originates from — so the preview can keep its
/// scroll position aligned with the editor and move the caret when a heading
/// is clicked. Parsing follows the GitHub dialect: tables, task lists,
/// strikethrough and bare-URL autolinks. Footnotes (`[^id]` / `[^id]: text`),
/// which md4c 0.5.2 has no native support for, are handled by a pre/post pass.
class Md4cRenderer
{
public:
    /// `markdown` is UTF-8 source. The result is a fragment with no
    /// `<html>`/`<head>`/`<body>` wrapper, ready to drop into a template.
    QString toHtml(const QString& markdown) const;
};

} // namespace hungryeditor
