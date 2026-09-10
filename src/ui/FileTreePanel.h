#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
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
    /// tree until the next setFiles().
    void setRoot(const QString& dir);

    /// Rebuild the tree from `absolutePaths`, folding each into nested directory
    /// items under the root. Paths outside the root are ignored. Directories
    /// sort before files, then by name; every directory starts expanded. The
    /// active filter is re-applied.
    void setFiles(const QStringList& absolutePaths);

signals:
    void fileActivated(const QString& path);

private:
    void onItemActivated(QTreeWidgetItem* item, int column);
    void applyFilter(const QString& text);
    /// Hide `item` and its descendants that do not match `needle` (already
    /// lower-cased); returns whether `item` stays visible.
    bool filterItem(QTreeWidgetItem* item, const QString& needle);
    void updatePlaceholder();

    QLineEdit* filter_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* empty_ = nullptr;
    QString root_;
};

} // namespace hungryeditor
