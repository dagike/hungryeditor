#pragma once

#include <QPair>
#include <QString>
#include <QVector>

namespace hungryeditor::frontmatter {

/// A parsed YAML front-matter header — the `---` fenced block that, by
/// convention, may lead a Markdown document. Only a flat `key: value` subset is
/// understood; that is all the preview card and the editor fold need.
struct FrontMatter
{
    bool present = false;                    ///< line 0 was `---` and a closing fence was found
    int firstLine = 0;                       ///< always 0 when present (the opening `---`)
    int lastLine = 0;                        ///< the closing `---` / `...` line, zero-based
    QVector<QPair<QString, QString>> fields; ///< top-level entries, in document order
};

/// Parse the leading front-matter block of `document`. Returns a value with
/// `present == false` (and no fields) when the document does not open with a
/// `---` line followed by a later `---` or `...` line.
///
/// Within the block: `key: value` lines become fields (keys trimmed, a matching
/// pair of surrounding quotes stripped from the value); `#` comment lines and
/// blank lines are skipped; an indented continuation line is folded onto the
/// previous value with a single space; `- item` block sequences and `[a, b]`
/// flow sequences collapse to a `, `-joined value.
FrontMatter parse(const QString& document);

} // namespace hungryeditor::frontmatter
