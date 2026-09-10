#pragma once

#include <QString>
#include <QVector>

namespace hungryeditor::outline {

/// One heading found in a Markdown document.
struct Heading
{
    int level = 1; ///< 1–6 (ATX `#` count, or 1/2 for a setext underline)
    QString text;  ///< the heading text, trimmed, with any closing `#` run removed
    int line = 0;  ///< zero-based source line the heading sits on
};

/// Extract the heading structure of `document`, in document order.
///
/// ATX headings (`#` … `######`, after at most three spaces, a space or the
/// line end required after the marker) and setext headings (a line of `=` or
/// `-` underlining a non-blank paragraph line) are recognised. Lines inside
/// fenced code blocks and a leading YAML front-matter block are skipped.
QVector<Heading> parse(const QString& document);

} // namespace hungryeditor::outline
