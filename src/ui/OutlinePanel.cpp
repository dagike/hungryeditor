#include "ui/OutlinePanel.h"

#include <QLabel>
#include <QScrollBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace hungryeditor {

namespace {
constexpr int kLineRole = Qt::UserRole;
} // namespace

OutlinePanel::OutlinePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(3);

    empty_ = new QLabel(tr("No headings"), this);
    empty_->setEnabled(false);
    empty_->setAlignment(Qt::AlignCenter);
    layout->addWidget(empty_);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setUniformRowHeights(true);
    tree_->setExpandsOnDoubleClick(false);
    layout->addWidget(tree_, 1);

    connect(tree_, &QTreeWidget::itemActivated, this, &OutlinePanel::onItemActivated);
    connect(tree_, &QTreeWidget::itemClicked, this, &OutlinePanel::onItemActivated);

    setHeadings({});
}

void OutlinePanel::setHeadings(const QVector<outline::Heading>& headings)
{
    const int scroll = tree_->verticalScrollBar()->value();

    tree_->clear();

    QVector<QTreeWidgetItem*> ancestors;
    QVector<int> levels;
    for (const outline::Heading& heading : headings) {
        while (!levels.isEmpty() && levels.back() >= heading.level) {
            levels.removeLast();
            ancestors.removeLast();
        }
        auto* item = ancestors.isEmpty() ? new QTreeWidgetItem(tree_)
                                         : new QTreeWidgetItem(ancestors.back());
        item->setText(0, heading.text.isEmpty() ? tr("(untitled)") : heading.text);
        item->setData(0, kLineRole, heading.line);
        item->setExpanded(true);
        ancestors.push_back(item);
        levels.push_back(heading.level);
    }

    const bool hasHeadings = !headings.isEmpty();
    tree_->setVisible(hasHeadings);
    empty_->setVisible(!hasHeadings);
    tree_->verticalScrollBar()->setValue(scroll);
}

void OutlinePanel::highlightLine(int caretLine)
{
    QTreeWidgetItem* best = nullptr;
    int bestLine = -1;
    for (QTreeWidgetItemIterator it(tree_); *it != nullptr; ++it) {
        const int line = (*it)->data(0, kLineRole).toInt();
        if (line <= caretLine && line > bestLine) {
            bestLine = line;
            best = *it;
        }
    }

    suppressActivation_ = true;
    tree_->setCurrentItem(best);
    suppressActivation_ = false;
}

void OutlinePanel::onItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    if (suppressActivation_ || item == nullptr) {
        return;
    }
    const QVariant line = item->data(0, kLineRole);
    if (line.isValid()) {
        emit headingActivated(line.toInt());
    }
}

} // namespace hungryeditor
