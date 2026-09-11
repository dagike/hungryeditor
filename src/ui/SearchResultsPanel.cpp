#include "ui/SearchResultsPanel.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace hungryeditor {

namespace {
constexpr int kPathRole = Qt::UserRole;
constexpr int kLineRole = Qt::UserRole + 1;
} // namespace

SearchResultsPanel::SearchResultsPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(3);

    summary_ = new QLabel(this);
    layout->addWidget(summary_);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderHidden(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setStretchLastSection(true);
    tree_->setAccessibleName(tr("Search results"));
    layout->addWidget(tree_, 1);

    connect(tree_, &QTreeWidget::itemActivated, this, &SearchResultsPanel::onItemActivated);
}

void SearchResultsPanel::showResults(const QString& query, const QList<FileSearchHit>& hits)
{
    hits_ = hits;
    tree_->clear();

    QTreeWidgetItem* fileItem = nullptr;
    QString currentPath;
    int fileCount = 0;
    for (const FileSearchHit& hit : hits) {
        if (hit.path != currentPath) {
            currentPath = hit.path;
            ++fileCount;
            fileItem = new QTreeWidgetItem(tree_);
            fileItem->setFirstColumnSpanned(true);
            fileItem->setText(0, QFileInfo(hit.path).fileName());
            fileItem->setToolTip(0, hit.path);
            fileItem->setExpanded(true);
        }
        auto* lineItem = new QTreeWidgetItem(fileItem);
        lineItem->setText(0, QString::number(hit.line + 1));
        lineItem->setText(1, hit.preview.trimmed());
        lineItem->setData(0, kPathRole, hit.path);
        lineItem->setData(0, kLineRole, hit.line);
    }

    summary_->setText(hits.isEmpty()
                          ? tr("No results for \"%1\"").arg(query)
                          : tr("%n result(s) in %1 file(s) for \"%2\"", nullptr, int(hits.size()))
                                .arg(fileCount)
                                .arg(query));
}

void SearchResultsPanel::clearResults()
{
    hits_.clear();
    tree_->clear();
    summary_->clear();
}

void SearchResultsPanel::activateResult(int index)
{
    if (index >= 0 && index < hits_.size()) {
        emit resultActivated(hits_.at(index).path, hits_.at(index).line);
    }
}

void SearchResultsPanel::onItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    const QVariant path = item->data(0, kPathRole);
    if (!path.isValid()) {
        return; // a file header row
    }
    emit resultActivated(path.toString(), item->data(0, kLineRole).toInt());
}

} // namespace hungryeditor
