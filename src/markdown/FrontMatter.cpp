#include "markdown/FrontMatter.h"

#include <QStringList>

namespace hungryeditor::frontmatter {

namespace {

bool isFence(const QString& line)
{
    const QString trimmed = line.trimmed();
    return trimmed == QStringLiteral("---") || trimmed == QStringLiteral("...");
}

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('"') && last == QLatin1Char('"')) ||
            (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            return value.mid(1, value.size() - 2);
        }
    }
    return value;
}

/// `[a, b, c]` -> `a, b, c`; a plain scalar is just unquoted.
QString scalarOrFlowSequence(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() >= 2 && trimmed.startsWith(QLatin1Char('[')) &&
        trimmed.endsWith(QLatin1Char(']'))) {
        QStringList items;
        for (const QString& piece : trimmed.mid(1, trimmed.size() - 2).split(QLatin1Char(','))) {
            const QString item = unquote(piece);
            if (!item.isEmpty()) {
                items << item;
            }
        }
        return items.join(QStringLiteral(", "));
    }
    return unquote(value);
}

int leadingWhitespace(const QString& line)
{
    int n = 0;
    while (n < line.size() && (line.at(n) == QLatin1Char(' ') || line.at(n) == QLatin1Char('\t'))) {
        ++n;
    }
    return n;
}

void appendToLastValue(FrontMatter& fm, const QString& extra, const QString& joiner)
{
    if (fm.fields.isEmpty() || extra.isEmpty()) {
        return;
    }
    QString& value = fm.fields.back().second;
    value = value.isEmpty() ? extra : value + joiner + extra;
}

} // namespace

FrontMatter parse(const QString& document)
{
    FrontMatter fm;

    const QStringList lines = document.split(QLatin1Char('\n'));
    if (lines.isEmpty() || lines.first().trimmed() != QStringLiteral("---")) {
        return fm;
    }

    int closing = -1;
    for (int i = 1; i < lines.size(); ++i) {
        if (isFence(lines.at(i))) {
            closing = i;
            break;
        }
    }
    if (closing < 0) {
        return fm;
    }

    fm.present = true;
    fm.firstLine = 0;
    fm.lastLine = closing;

    for (int i = 1; i < closing; ++i) {
        const QString& raw = lines.at(i);
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (trimmed == QStringLiteral("-") || trimmed.startsWith(QLatin1String("- "))) {
            appendToLastValue(fm, unquote(trimmed.mid(1).trimmed()), QStringLiteral(", "));
            continue;
        }

        const qsizetype colon = trimmed.indexOf(QLatin1Char(':'));
        if (colon < 0 || (leadingWhitespace(raw) > 0 && !fm.fields.isEmpty())) {
            // A wrapped scalar or a nested line we do not model: fold it in.
            appendToLastValue(fm, trimmed, QStringLiteral(" "));
            continue;
        }

        const QString key = trimmed.left(colon).trimmed();
        if (key.isEmpty()) {
            continue;
        }
        const QString rest = trimmed.mid(colon + 1).trimmed();
        fm.fields.push_back({key, rest.isEmpty() ? QString() : scalarOrFlowSequence(rest)});
    }

    return fm;
}

} // namespace hungryeditor::frontmatter
