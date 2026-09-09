#pragma once

#include <QList>
#include <QString>

namespace hungryeditor {

/// One entry in the recent-files list: an absolute path and whether the user
/// pinned it so it is never evicted.
struct RecentFile
{
    QString path;
    bool pinned = false;
};

/// The recent-files list, persisted to a single recent.json. Pinned entries
/// are held in pin order ahead of the unpinned ones, which run most-recent
/// first and are capped at kMaxRecent. The file location is injected so the
/// running app and the test suite never share one. Paths are stored absolute.
class RecentFiles
{
public:
    /// Cap on the number of unpinned entries kept.
    static constexpr int kMaxRecent = 12;

    explicit RecentFiles(QString filePath);

    QString filePath() const { return filePath_; }

    /// Move `path` to the front of the unpinned entries (a pinned entry keeps
    /// its slot), dropping any duplicate and trimming to the cap.
    void noteOpened(const QString& path);

    /// Drop `path` from the list, pinned or not.
    void forget(const QString& path);

    /// Pin or unpin `path`; pinning a path that is not listed adds it.
    void setPinned(const QString& path, bool pinned);
    bool isPinned(const QString& path) const;

    /// The entries to show: pinned first in pin order, then the unpinned ones
    /// newest-first.
    QList<RecentFile> entries() const { return entries_; }

    /// Drop every unpinned entry.
    void clearUnpinned();

    /// Replace the in-memory list with what is on disk (empty when absent).
    void load();

    /// Write the list to disk atomically, creating the parent directory.
    bool save() const;

private:
    int indexOf(const QString& absolutePath) const;
    int pinnedCount() const;
    void trimUnpinned();

    QString filePath_;
    QList<RecentFile> entries_; ///< pinned block first, then unpinned newest-first
};

} // namespace hungryeditor
