#include "markdown/Outline.h"

#include <QStringList>

#include "markdown/FrontMatter.h"

namespace hungryeditor::outline {

namespace {

int leadingSpaces(const QString& line)
{
    int n = 0;
    while (n < line.size() && line.at(n) == QLatin1Char(' ')) {
        ++n;
    }
    return n;
}

/// True when `line` opens or closes a fenced code block (``` or ~~~, three or
/// more, after at most three spaces).
bool isCodeFence(const QString& line)
{
    const int indent = leadingSpaces(line);
    if (indent > 3 || indent >= line.size()) {
        return false;
    }
    const QChar marker = line.at(indent);
    if (marker != QLatin1Char('`') && marker != QLatin1Char('~')) {
        return false;
    }
    int run = 0;
    while (indent + run < line.size() && line.at(indent + run) == marker) {
        ++run;
    }
    return run >= 3;
}

/// Drop a trailing run of `#` (an ATX closing sequence) plus the space before it.
QString stripClosingHashes(QString text)
{
    text = text.trimmed();
    int hashes = 0;
    while (hashes < text.size() && text.at(text.size() - 1 - hashes) == QLatin1Char('#')) {
        ++hashes;
    }
    if (hashes == 0) {
        return text;
    }
    const qsizetype before = text.size() - hashes - 1;
    if (before < 0) {
        return QString(); // the text was only `#`s
    }
    if (text.at(before) == QLatin1Char(' ') || text.at(before) == QLatin1Char('\t')) {
        return text.left(before).trimmed();
    }
    return text; // not space-separated: the `#`s are literal
}

/// A line made up entirely of `=` or entirely of `-` is a setext underline.
int setextLevel(const QString& trimmed)
{
    if (trimmed.isEmpty()) {
        return 0;
    }
    const QChar first = trimmed.at(0);
    if (first != QLatin1Char('=') && first != QLatin1Char('-')) {
        return 0;
    }
    for (const QChar ch : trimmed) {
        if (ch != first) {
            return 0;
        }
    }
    return first == QLatin1Char('=') ? 1 : 2;
}

} // namespace

QVector<Heading> parse(const QString& document)
{
    QVector<Heading> headings;

    const QStringList lines = document.split(QLatin1Char('\n'));
    const frontmatter::FrontMatter front = frontmatter::parse(document);
    const int firstBody = front.present ? front.lastLine + 1 : 0;

    bool inFence = false;
    for (int i = firstBody; i < lines.size(); ++i) {
        const QString& raw = lines.at(i);

        if (isCodeFence(raw)) {
            inFence = !inFence;
            continue;
        }
        if (inFence) {
            continue;
        }

        const int indent = leadingSpaces(raw);
        if (indent <= 3 && indent < raw.size() && raw.at(indent) == QLatin1Char('#')) {
            int level = 0;
            while (indent + level < raw.size() && raw.at(indent + level) == QLatin1Char('#')) {
                ++level;
            }
            const int after = indent + level;
            if (level >= 1 && level <= 6 &&
                (after == raw.size() || raw.at(after) == QLatin1Char(' ') ||
                 raw.at(after) == QLatin1Char('\t'))) {
                headings.push_back({level, stripClosingHashes(raw.mid(after)), i});
                continue;
            }
        }

        const QString trimmed = raw.trimmed();
        const int level = (indent <= 3) ? setextLevel(trimmed) : 0;
        if (level != 0) {
            const int prev = i - 1;
            const bool prevWasHeading = !headings.isEmpty() && headings.back().line == prev;
            if (prev >= firstBody && !prevWasHeading && !isCodeFence(lines.at(prev))) {
                const QString title = lines.at(prev).trimmed();
                if (!title.isEmpty()) {
                    headings.push_back({level, title, prev});
                }
            }
        }
    }

    return headings;
}

} // namespace hungryeditor::outline
