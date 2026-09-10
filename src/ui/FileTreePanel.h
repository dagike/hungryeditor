#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QMenu;
class QPoint;
class QTreeWidget;
class QTreeWidgetItem;

namespace hungryeditor {

/// Left-side panel showing a workspace folder as a tree of directories and
/// files, with a filter box. The file list is supplied from outside (the same
/// background scan that feeds quick open); activating a leaf asks the window to
/// open that path.
class FileTreePanel : public QWidget
{
    Q_OBJECT

public:
    explicit FileTreePanel(QWidget* parent = nullptr);

    QString root() const { return root_; }

    /// Set the directory every supplied path is shown relative to. Clears the
    /// tree and any restored view state until the next setFiles().
    void setRoot(const QString& dir);

    /// Rebuild the tree from `absolutePaths`, folding each into nested directory
    /// items under the root. Paths outside the root are ignored. Directories
    /// sort before files, then by name. The restored view state (see
    /// applyState) is re-applied; with none, every directory starts expanded.
    void setFiles(const QStringList& absolutePaths);

    /// Restore a saved view: the filter text, and — when `hasExpandedList` —
    /// exactly the directories in `expandedDirs` (plus their ancestors) are
    /// expanded and the rest collapsed. Persists across setFiles() rebuilds.
    void applyState(const QString& filter, const QStringList& expandedDirs, bool hasExpandedList);

    QString filterText() const;

    /// Directory paths (relative to the root) that are currently expanded.
    QStringList expandedDirectories() const;

    /// The right-click menu for `item` (nullptr ⇒ a click on empty space,
    /// targeting the workspace root), parented to this panel, or nullptr when
    /// no folder is open. Triggering an entry emits the matching request
    /// signal. Exposed so tests can drive it without a modal exec().
    QMenu* contextMenuFor(QTreeWidgetItem* item);

signals:
    void fileActivated(const QString& path);
    void createFileRequested(const QString& parentDir);
    void createFolderRequested(const QString& parentDir);
    void renameRequested(const QString& path, bool isDirectory);
    void deleteRequested(const QString& path, bool isDirectory);

private:
    void onItemActivated(QTreeWidgetItem* item, int column);
    void showContextMenu(const QPoint& pos);
    void applyFilter(const QString& text);
    /// Hide `item` and its descendants that do not match `needle` (already
    /// lower-cased); returns whether `item` stays visible.
    bool filterItem(QTreeWidgetItem* item, const QString& needle);
    void applyExpansion();
    void updatePlaceholder();

    QLineEdit* filter_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* empty_ = nullptr;
    QString root_;
    QStringList restoredExpanded_;
    bool hasRestoredExpanded_ = false;
};

} // namespace hungryeditor
