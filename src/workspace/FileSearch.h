#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace hungryeditor {

/// How a find-in-files query matches (mirrors the editor's find options).
struct FileSearchOptions
{
    bool matchCase = false;
    bool wholeWord = false;
    bool regex = false;
};

/// One matching line found by searchDirectory().
struct FileSearchHit
{
    QString path;
    int line = 0;    ///< zero-based line number
    int column = 0;  ///< zero-based column of the match start
    QString preview; ///< the whole matching line, newline stripped
};

/// Caps that keep a runaway search bounded.
struct FileSearchLimits
{
    int maxFiles = 5000;
    qint64 maxFileBytes = 4LL * 1024 * 1024;
    int maxHits = 2000;
};

/// Recursively search the UTF-8 text files under `directory` for `query`.
/// Non-text extensions, oversized files and files containing NUL bytes are
/// skipped. An empty query or an invalid regex yields no results. Hits come
/// back in directory-walk order.
QList<FileSearchHit> searchDirectory(const QString& directory, const QString& query,
                                     const FileSearchOptions& options,
                                     const FileSearchLimits& limits = {});

} // namespace hungryeditor
