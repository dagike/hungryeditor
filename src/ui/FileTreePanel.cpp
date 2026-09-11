#include "ui/FileTreePanel.h"

#include <algorithm>

#include <QAction>
#include <QDir>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSet>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace hungryeditor {

namespace {

constexpr int kPathRole = Qt::UserRole; ///< set on file leaves only

bool isDirectory(const QTreeWidgetItem* item)
{
    return !item->data(0, kPathRole).isValid();
}

/// The item's path relative to the tree root, "/"-joined.
QString itemPath(const QTreeWidgetItem* item)
{
    QStringList parts;
    for (const QTreeWidgetItem* node = item; node != nullptr; node = node->parent()) {
        parts.prepend(node->text(0));
    }
    return parts.join(QLatin1Char('/'));
}

void sortChildren(QTreeWidgetItem* parent)
{
    QList<QTreeWidgetItem*> children = parent->takeChildren();
    std::sort(children.begin(), children.end(), [](QTreeWidgetItem* a, QTreeWidgetItem* b) {
        if (isDirectory(a) != isDirectory(b)) {
            return isDirectory(a); // directories first
        }
        return a->text(0).localeAwareCompare(b->text(0)) < 0;
    });
    parent->addChildren(children);
    for (QTreeWidgetItem* child : children) {
        sortChildren(child);
    }
}

} // namespace

FileTreePanel::FileTreePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(3);

    filter_ = new QLineEdit(this);
    filter_->setPlaceholderText(tr("Filter files…"));
    filter_->setClearButtonEnabled(true);
    filter_->setAccessibleName(tr("Filter files"));
    layout->addWidget(filter_);
    connect(filter_, &QLineEdit::textChanged, this, &FileTreePanel::applyFilter);

    empty_ = new QLabel(this);
    empty_->setEnabled(false);
    empty_->setAlignment(Qt::AlignCenter);
    empty_->setWordWrap(true);
    layout->addWidget(empty_);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setUniformRowHeights(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setAccessibleName(tr("File tree"));
    layout->addWidget(tree_, 1);
    connect(tree_, &QTreeWidget::itemActivated, this, &FileTreePanel::onItemActivated);
    connect(tree_, &QWidget::customContextMenuRequested, this, &FileTreePanel::showContextMenu);

    updatePlaceholder();
}

void FileTreePanel::setRoot(const QString& dir)
{
    if (dir == root_) {
        return;
    }
    root_ = dir;
    restoredExpanded_.clear();
    hasRestoredExpanded_ = false;
    {
        const QSignalBlocker block(filter_);
        filter_->clear();
    }
    tree_->clear();
    updatePlaceholder();
}

void FileTreePanel::setFiles(const QStringList& absolutePaths)
{
    tree_->clear();

    if (!root_.isEmpty()) {
        const QDir root(root_);
        QHash<QString, QTreeWidgetItem*> dirs; // relative dir path -> item

        for (const QString& path : absolutePaths) {
            const QString relative = root.relativeFilePath(path);
            if (relative.isEmpty() || relative.startsWith(QLatin1String("../")) ||
                relative.startsWith(QLatin1Char('/'))) {
                continue; // outside the root
            }

            const QStringList segments = relative.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            if (segments.isEmpty()) {
                continue;
            }

            QTreeWidgetItem* parent = tree_->invisibleRootItem();
            QString sofar;
            for (int i = 0; i < segments.size() - 1; ++i) {
                sofar = sofar.isEmpty() ? segments[i] : sofar + QLatin1Char('/') + segments[i];
                QTreeWidgetItem*& dir = dirs[sofar];
                if (dir == nullptr) {
                    dir = new QTreeWidgetItem(parent);
                    dir->setText(0, segments[i]);
                }
                parent = dir;
            }

            auto* leaf = new QTreeWidgetItem(parent);
            leaf->setText(0, segments.last());
            leaf->setData(0, kPathRole, path);
        }

        sortChildren(tree_->invisibleRootItem());
    }

    applyExpansion();
    applyFilter(filter_->text());
    updatePlaceholder();
}

void FileTreePanel::applyState(const QString& filter, const QStringList& expandedDirs,
                               bool hasExpandedList)
{
    restoredExpanded_ = expandedDirs;
    hasRestoredExpanded_ = hasExpandedList;
    {
        const QSignalBlocker block(filter_);
        filter_->setText(filter);
    }
    applyExpansion();
    applyFilter(filter_->text());
}

void FileTreePanel::applyExpansion()
{
    if (!hasRestoredExpanded_) {
        tree_->expandAll();
        return;
    }

    QSet<QString> wanted;
    for (const QString& dir : restoredExpanded_) {
        QString sofar;
        for (const QString& segment : dir.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            sofar = sofar.isEmpty() ? segment : sofar + QLatin1Char('/') + segment;
            wanted.insert(sofar);
        }
    }

    for (QTreeWidgetItemIterator it(tree_); *it != nullptr; ++it) {
        if (isDirectory(*it)) {
            (*it)->setExpanded(wanted.contains(itemPath(*it)));
        }
    }
}

void FileTreePanel::applyFilter(const QString& text)
{
    const QString needle = text.trimmed().toLower();
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        filterItem(tree_->topLevelItem(i), needle);
    }
}

bool FileTreePanel::filterItem(QTreeWidgetItem* item, const QString& needle)
{
    if (!isDirectory(item)) {
        const bool visible = needle.isEmpty() || item->text(0).toLower().contains(needle);
        item->setHidden(!visible);
        return visible;
    }

    bool anyVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        anyVisible = filterItem(item->child(i), needle) || anyVisible;
    }
    item->setHidden(!anyVisible);
    return anyVisible;
}

QString FileTreePanel::filterText() const
{
    return filter_->text();
}

QStringList FileTreePanel::expandedDirectories() const
{
    QStringList dirs;
    for (QTreeWidgetItemIterator it(tree_); *it != nullptr; ++it) {
        if (isDirectory(*it) && (*it)->isExpanded()) {
            dirs.append(itemPath(*it));
        }
    }
    return dirs;
}

void FileTreePanel::updatePlaceholder()
{
    const bool hasFiles = tree_->topLevelItemCount() > 0;
    tree_->setVisible(hasFiles);
    empty_->setVisible(!hasFiles);
    empty_->setText(root_.isEmpty() ? tr("No folder open") : tr("No files"));
}

void FileTreePanel::onItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    if (item == nullptr) {
        return;
    }
    const QVariant path = item->data(0, kPathRole);
    if (path.isValid()) {
        emit fileActivated(path.toString());
    }
}

QMenu* FileTreePanel::contextMenuFor(QTreeWidgetItem* item)
{
    if (root_.isEmpty()) {
        return nullptr;
    }

    QString parentDir = root_;
    QString targetPath;
    bool targetIsDir = false;
    if (item != nullptr && isDirectory(item)) {
        targetPath = QDir(root_).absoluteFilePath(itemPath(item));
        targetIsDir = true;
        parentDir = targetPath;
    } else if (item != nullptr) {
        targetPath = item->data(0, kPathRole).toString();
        // Derive the parent from the tree, not QFileInfo::absolutePath() — the
        // latter drive-qualifies a bare POSIX path on Windows.
        QTreeWidgetItem* parentItem = item->parent();
        parentDir =
            parentItem != nullptr ? QDir(root_).absoluteFilePath(itemPath(parentItem)) : root_;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("New File…")), &QAction::triggered, this,
            [this, parentDir] { emit createFileRequested(parentDir); });
    connect(menu->addAction(tr("New Folder…")), &QAction::triggered, this,
            [this, parentDir] { emit createFolderRequested(parentDir); });
    if (!targetPath.isEmpty()) {
        menu->addSeparator();
        connect(menu->addAction(tr("Rename…")), &QAction::triggered, this,
                [this, targetPath, targetIsDir] { emit renameRequested(targetPath, targetIsDir); });
        connect(menu->addAction(tr("Delete")), &QAction::triggered, this,
                [this, targetPath, targetIsDir] { emit deleteRequested(targetPath, targetIsDir); });
    }
    return menu;
}

void FileTreePanel::showContextMenu(const QPoint& pos)
{
    if (QMenu* menu = contextMenuFor(tree_->itemAt(pos))) {
        menu->popup(tree_->viewport()->mapToGlobal(pos));
    }
}

} // namespace hungryeditor
