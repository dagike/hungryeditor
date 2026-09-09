#include "workspace/FileSearch.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace hungryeditor {

namespace {

const QStringList kTextGlobs{QStringLiteral("*.md"),  QStringLiteral("*.markdown"),
                             QStringLiteral("*.mkd"), QStringLiteral("*.mdown"),
                             QStringLiteral("*.txt"), QStringLiteral("*.text")};

QRegularExpression buildPattern(const QString& query, const FileSearchOptions& options)
{
    QString source = options.regex ? query : QRegularExpression::escape(query);
    if (options.wholeWord) {
        source = QStringLiteral("\\b(?:%1)\\b").arg(source);
    }
    QRegularExpression::PatternOptions flags = QRegularExpression::NoPatternOption;
    if (!options.matchCase) {
        flags |= QRegularExpression::CaseInsensitiveOption;
    }
    return QRegularExpression(source, flags);
}

} // namespace

QList<FileSearchHit> searchDirectory(const QString& directory, const QString& query,
                                     const FileSearchOptions& options,
                                     const FileSearchLimits& limits)
{
    QList<FileSearchHit> hits;
    if (query.isEmpty()) {
        return hits;
    }
    const QRegularExpression pattern = buildPattern(query, options);
    if (!pattern.isValid()) {
        return hits;
    }

    int filesSeen = 0;
    QDirIterator it(directory, kTextGlobs, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && filesSeen < limits.maxFiles && hits.size() < limits.maxHits) {
        const QString path = it.next();
        ++filesSeen;

        const QFileInfo info(path);
        if (info.size() > limits.maxFileBytes) {
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray bytes = file.readAll();
        if (bytes.contains('\0')) {
            continue; // looks binary
        }

        const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));
        for (int lineNo = 0; lineNo < lines.size() && hits.size() < limits.maxHits; ++lineNo) {
            const QString& line = lines.at(lineNo);
            auto matches = pattern.globalMatch(line);
            while (matches.hasNext() && hits.size() < limits.maxHits) {
                const QRegularExpressionMatch match = matches.next();
                if (match.capturedLength() == 0) {
                    break; // zero-width match: avoid an infinite run on one line
                }
                hits.append({path, lineNo, int(match.capturedStart()), line});
            }
        }
    }
    return hits;
}

} // namespace hungryeditor
