#pragma once

#include <QString>
#include <QStringList>

namespace hungryeditor {

/// The remembered view of one workspace folder: the sidebar filter text and
/// which directories were left expanded. `hasExpandedList` is false until the
/// folder has been saved at least once, so a fresh workspace keeps the panel's
/// expand-everything default instead of collapsing to an empty list.
struct WorkspaceState
{
    QString filter;
    QStringList expandedDirs; ///< directory paths relative to the folder root
    bool hasExpandedList = false;
};

/// Reads and writes per-folder workspace state in a single workspaces.json (a
/// map from absolute folder path to its WorkspaceState). The file location is
/// injected so the running app and the test suite never share one.
class WorkspaceStore
{
public:
    explicit WorkspaceStore(QString filePath);

    QString filePath() const { return filePath_; }

    /// The stored state for `folder`, or a default (hasExpandedList == false)
    /// when the folder or the file is absent.
    WorkspaceState load(const QString& folder) const;

    /// Write `state` for `folder`, leaving other folders' entries untouched.
    /// Creates the parent directory.
    bool save(const QString& folder, const WorkspaceState& state) const;

private:
    QString filePath_;
};

} // namespace hungryeditor
