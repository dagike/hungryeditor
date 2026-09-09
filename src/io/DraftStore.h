#pragma once

#include <QList>
#include <QSet>
#include <QString>

#include "io/TextFile.h" // Encoding, LineEnding

namespace hungryeditor {

/// One recovered buffer: everything needed to put an unsaved document back on
/// screen after a crash.
struct Draft
{
    QString id;
    QString originalPath; ///< empty for a buffer that was never saved
    QString text;         ///< newlines normalised to "\n"
    Encoding encoding = Encoding::Utf8;
    LineEnding lineEnding = LineEnding::Lf;
};

/// A directory of autosaved drafts — one JSON file per buffer, named by its id.
/// The directory is created on demand; its location is injected so the running
/// app and the test suite never share one.
class DraftStore
{
public:
    explicit DraftStore(QString directory);

    QString directory() const { return directory_; }

    /// Atomically write (or replace) the draft for `draft.id`.
    bool write(const Draft& draft);

    /// Delete the draft file for `id`, if it exists.
    void remove(const QString& id);

    /// Every draft currently on disk, oldest first.
    QList<Draft> loadAll() const;

    /// Delete every draft file.
    void clear();

    /// Delete every draft file whose id is not in `ids` — used after a
    /// session restore to drop drafts left by buffers that are no longer open.
    void retainOnly(const QSet<QString>& ids);

private:
    QString directory_;
};

} // namespace hungryeditor
