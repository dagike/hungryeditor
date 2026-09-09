#include "ui/FindReplaceBar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace hungryeditor {

FindReplaceBar::FindReplaceBar(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 4, 6, 4);
    outer->setSpacing(3);

    // --- find row -----------------------------------------------------------
    auto* findRow = new QHBoxLayout;
    findRow->setSpacing(3);

    queryEdit_ = new QLineEdit(this);
    queryEdit_->setPlaceholderText(tr("Find"));
    queryEdit_->setClearButtonEnabled(true);

    auto* prevButton = new QToolButton(this);
    prevButton->setText(QStringLiteral("▲"));
    prevButton->setToolTip(tr("Previous match (Shift+Enter)"));
    auto* nextButton = new QToolButton(this);
    nextButton->setText(QStringLiteral("▼"));
    nextButton->setToolTip(tr("Next match (Enter)"));

    caseButton_ = addToggle(QStringLiteral("Aa"), tr("Match case"));
    wordButton_ = addToggle(QStringLiteral("W"), tr("Whole word"));
    regexButton_ = addToggle(QStringLiteral(".*"), tr("Regular expression"));

    countLabel_ = new QLabel(this);
    countLabel_->setMinimumWidth(72);

    auto* closeButton = new QToolButton(this);
    closeButton->setText(QStringLiteral("✕"));
    closeButton->setToolTip(tr("Close (Esc)"));

    findRow->addWidget(queryEdit_, 1);
    findRow->addWidget(prevButton);
    findRow->addWidget(nextButton);
    findRow->addWidget(caseButton_);
    findRow->addWidget(wordButton_);
    findRow->addWidget(regexButton_);
    findRow->addWidget(countLabel_);
    findRow->addWidget(closeButton);
    outer->addLayout(findRow);

    // --- replace row ------------------------------------------------------
    replaceRow_ = new QWidget(this);
    auto* replaceLayout = new QHBoxLayout(replaceRow_);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(3);

    replaceEdit_ = new QLineEdit(replaceRow_);
    replaceEdit_->setPlaceholderText(tr("Replace"));
    replaceEdit_->setClearButtonEnabled(true);

    auto* replaceOneButton = new QToolButton(replaceRow_);
    replaceOneButton->setText(tr("Replace"));
    auto* replaceAllButton = new QToolButton(replaceRow_);
    replaceAllButton->setText(tr("All"));

    replaceLayout->addWidget(replaceEdit_, 1);
    replaceLayout->addWidget(replaceOneButton);
    replaceLayout->addWidget(replaceAllButton);
    outer->addWidget(replaceRow_);

    // --- wiring ----------------------------------------------------------
    connect(queryEdit_, &QLineEdit::returnPressed, this, [this] { emit findRequested(true); });
    connect(queryEdit_, &QLineEdit::textChanged, this, &FindReplaceBar::queryChanged);
    connect(nextButton, &QToolButton::clicked, this, [this] { emit findRequested(true); });
    connect(prevButton, &QToolButton::clicked, this, [this] { emit findRequested(false); });
    connect(closeButton, &QToolButton::clicked, this, &FindReplaceBar::dismissed);
    for (QToolButton* toggle : {caseButton_, wordButton_, regexButton_}) {
        connect(toggle, &QToolButton::toggled, this, &FindReplaceBar::queryChanged);
    }
    connect(replaceEdit_, &QLineEdit::returnPressed, this, &FindReplaceBar::replaceOneRequested);
    connect(replaceOneButton, &QToolButton::clicked, this, &FindReplaceBar::replaceOneRequested);
    connect(replaceAllButton, &QToolButton::clicked, this, &FindReplaceBar::replaceAllRequested);

    hide();
}

QToolButton* FindReplaceBar::addToggle(const QString& label, const QString& tooltip)
{
    auto* button = new QToolButton(this);
    button->setText(label);
    button->setToolTip(tooltip);
    button->setCheckable(true);
    return button;
}

void FindReplaceBar::reveal(bool withReplace)
{
    replaceRow_->setVisible(withReplace);
    show();
    queryEdit_->setFocus();
    queryEdit_->selectAll();
}

QString FindReplaceBar::query() const
{
    return queryEdit_->text();
}

QString FindReplaceBar::replacement() const
{
    return replaceEdit_->text();
}

void FindReplaceBar::setQuery(const QString& text)
{
    queryEdit_->setText(text);
}

void FindReplaceBar::setReplacement(const QString& text)
{
    replaceEdit_->setText(text);
}

Editor::SearchOptions FindReplaceBar::options() const
{
    return {caseButton_->isChecked(), wordButton_->isChecked(), regexButton_->isChecked()};
}

void FindReplaceBar::setMatchCount(int count)
{
    countLabel_->setText(query().isEmpty() ? QString() : tr("%n match(es)", nullptr, count));
}

void FindReplaceBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit dismissed();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit findRequested(!(event->modifiers() & Qt::ShiftModifier));
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace hungryeditor
