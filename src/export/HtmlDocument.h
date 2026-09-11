#pragma once

#include <QString>

#include "theme/Theme.h"

namespace hungryeditor::htmlexport {

/// Inputs to build(), beyond the Markdown source itself.
struct Options
{
    QString title;       ///< <title> text, already meant for display (unescaped)
    QString documentDir; ///< resolves relative <img> paths; empty for an unsaved buffer
    Theme theme = Theme::builtin();
};

/// Render `markdown` into a complete, self-contained HTML document: the same
/// fragment the live preview shows, wrapped in a real `<html>` with the theme
/// CSS inlined, local images turned into `data:` URIs, and — only when the
/// document actually uses them — KaTeX and Mermaid bundled and rendered by a
/// small inline script. A plain prose document carries no such extras. The
/// result opens correctly in any browser with no other files alongside it.
QString build(const QString& markdown, const Options& options);

} // namespace hungryeditor::htmlexport
