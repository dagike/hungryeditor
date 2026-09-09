#include "ui/CommandPalette.h"

#include <algorithm>
#include <vector>

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

#include "ui/FuzzyMatch.h"

namespace hungryeditor {

namespace {
constexpr int kIdRole = Qt::UserRole;
constexpr int kPaletteWidth = 480;
constexpr int kMaxRows = 12;
} // namespace

CommandPalette::CommandPalette(QWidget* parent) : QWidget(parent)
{
    setAutoFillBackground(true);
    setFixedWidth(kPaletteWidth);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    query_ = new QLineEdit(this);
    query_->setPlaceholderText(tr("Run a command…"));
    query_->installEventFilter(this);
    layout->addWidget(query_);

    list_ = new QListWidget(this);
    list_->setUniformItemSizes(true);
    layout->addWidget(list_);

    connect(query_, &QLineEdit::textChanged, this, &CommandPalette::refilter);
    connect(list_, &QListWidget::itemActivated, this, [this] { accept(); });

    hide();
}

void CommandPalette::setCommands(const QList<Command>& commands)
{
    commands_ = commands;
}

void CommandPalette::open()
{
    query_->clear();
    refilter();
    if (const QWidget* host = parentWidget()) {
        move((host->width() - width()) / 2, 48);
    }
    raise();
    show();
    query_->setFocus();
}

void CommandPalette::refilter()
{
    const QString pattern = query_->text();

    struct Scored
    {
        int score;
        const Command* command;
    };
    std::vector<Scored> hits;
    for (const Command& command : commands_) {
        const FuzzyResult result = fuzzyMatch(pattern, command.title);
        if (result.matched) {
            hits.push_back({result.score, &command});
        }
    }
    std::stable_sort(hits.begin(), hits.end(),
                     [](const Scored& a, const Scored& b) { return a.score > b.score; });

    list_->clear();
    for (const Scored& hit : hits) {
        const QString label =
            hit.command->shortcut.isEmpty()
                ? hit.command->title
                : hit.command->title + QStringLiteral("\t") + hit.command->shortcut;
        auto* item = new QListWidgetItem(label, list_);
        item->setData(kIdRole, hit.command->id);
    }
    if (list_->count() > 0) {
        list_->setCurrentRow(0);
    }
    list_->setVisible(list_->count() > 0);

    const int rows = std::min(list_->count(), kMaxRows);
    list_->setFixedHeight(rows > 0 ? rows * list_->sizeHintForRow(0) + 4 : 0);
    adjustSize();
}

void CommandPalette::accept()
{
    QListWidgetItem* item = list_->currentItem();
    if (item != nullptr) {
        hide();
        emit commandChosen(item->data(kIdRole).toString());
    }
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == query_ && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            accept();
            return true;
        case Qt::Key_Down:
            list_->setCurrentRow(std::min(list_->currentRow() + 1, list_->count() - 1));
            return true;
        case Qt::Key_Up:
            list_->setCurrentRow(std::max(list_->currentRow() - 1, 0));
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace hungryeditor
