#include "ui/TabSwitcher.h"

#include <algorithm>

#include <QKeyEvent>
#include <QListWidget>
#include <QVBoxLayout>

namespace hungryeditor {

namespace {
constexpr int kWidth = 420;
constexpr int kMaxRows = 12;
} // namespace

TabSwitcher::TabSwitcher(QWidget* parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setFixedWidth(kWidth);
    setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(0);

    list_ = new QListWidget(this);
    list_->setUniformItemSizes(true);
    list_->setFocusPolicy(Qt::NoFocus); // the frame keeps focus so Ctrl-release lands here
    layout->addWidget(list_);

    hide();
}

void TabSwitcher::present(const QList<Entry>& entries, int startRow)
{
    list_->clear();
    for (const Entry& entry : entries) {
        QString label = entry.modified ? QStringLiteral("* ") + entry.title : entry.title;
        if (!entry.detail.isEmpty()) {
            label += QStringLiteral("\t") + entry.detail;
        }
        new QListWidgetItem(label, list_);
    }
    if (list_->count() == 0) {
        return;
    }

    list_->setCurrentRow(qBound(0, startRow, list_->count() - 1));
    resizeToContents();
    if (const QWidget* host = parentWidget()) {
        move((host->width() - width()) / 2, host->height() / 4);
    }
    active_ = true;
    raise();
    show();
    setFocus();
}

void TabSwitcher::resizeToContents()
{
    const int rows = std::min(list_->count(), kMaxRows);
    const int rowHeight = list_->count() > 0 ? list_->sizeHintForRow(0) : 0;
    list_->setFixedHeight(rows > 0 ? rows * rowHeight + 2 * list_->frameWidth() : 0);
    adjustSize();
}

int TabSwitcher::currentRow() const
{
    return list_->currentRow();
}

void TabSwitcher::selectNext()
{
    const int n = list_->count();
    if (n > 0) {
        list_->setCurrentRow((list_->currentRow() + 1) % n);
    }
}

void TabSwitcher::selectPrevious()
{
    const int n = list_->count();
    if (n > 0) {
        list_->setCurrentRow((list_->currentRow() + n - 1) % n);
    }
}

void TabSwitcher::commit()
{
    if (!active_) {
        return;
    }
    active_ = false;
    hide();
    emit accepted(list_->currentRow());
}

void TabSwitcher::cancel()
{
    if (!active_) {
        return;
    }
    active_ = false;
    hide();
    emit cancelled();
}

void TabSwitcher::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        cancel();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        commit();
        return;
    case Qt::Key_Tab:
    case Qt::Key_Down:
        selectNext();
        return;
    case Qt::Key_Backtab:
    case Qt::Key_Up:
        selectPrevious();
        return;
    default:
        break;
    }
    QFrame::keyPressEvent(event);
}

void TabSwitcher::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Control && !event->isAutoRepeat()) {
        commit();
        return;
    }
    QFrame::keyReleaseEvent(event);
}

void TabSwitcher::focusOutEvent(QFocusEvent* event)
{
    QFrame::focusOutEvent(event);
    cancel();
}

} // namespace hungryeditor
