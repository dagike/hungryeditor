#include "ui/FileTreePanel.h"

#include <algorithm>

#include <QDir>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace hungryeditor {

namespace {

constexpr int kPathRole = Qt::UserRole; ///< set on file leaves only

bool isDirectory(const QTreeWidgetItem* item)
{
    return !item->data(0, kPathRole).isValid();
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
    layout->addWidget(tree_, 1);
    connect(tree_, &QTreeWidget::itemActivated, this, &FileTreePanel::onItemActivated);

    updatePlaceholder();
}

void FileTreePanel::setRoot(const QString& dir)
{
    if (dir == root_) {
        return;
    }
    root_ = dir;
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
                    dir->setExpanded(true);
                }
                parent = dir;
            }

            auto* leaf = new QTreeWidgetItem(parent);
            leaf->setText(0, segments.last());
            leaf->setData(0, kPathRole, path);
        }

        sortChildren(tree_->invisibleRootItem());
        tree_->expandAll();
    }

    applyFilter(filter_->text());
    updatePlaceholder();
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

} // namespace hungryeditor
