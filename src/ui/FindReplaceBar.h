#pragma once

#include <QWidget>

#include "editor/Editor.h" // Editor::SearchOptions

class QLabel;
class QLineEdit;
class QToolButton;

namespace hungryeditor {

/// A slim find / find-and-replace bar that docks above the editor. It owns no
/// search logic — it emits intent and the window drives the Editor.
class FindReplaceBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindReplaceBar(QWidget* parent = nullptr);

    /// Reveal the bar (find row only) and focus the query field.
    void reveal(bool withReplace);

    QString query() const;
    QString replacement() const;
    /// Prefill the query field (e.g. from the editor's current selection).
    void setQuery(const QString& text);
    void setReplacement(const QString& text);
    Editor::SearchOptions options() const;

    /// Update the "N matches" label.
    void setMatchCount(int count);

signals:
    /// Enter / the arrow buttons: move to the next (or previous) match.
    void findRequested(bool forward);
    /// Replace the current match, then advance.
    void replaceOneRequested();
    /// Replace every match.
    void replaceAllRequested();
    /// The query text or an option toggle changed.
    void queryChanged();
    /// Esc or the close button: the window should hide the bar.
    void dismissed();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QToolButton* addToggle(const QString& label, const QString& tooltip);

    QLineEdit* queryEdit_ = nullptr;
    QLineEdit* replaceEdit_ = nullptr;
    QWidget* replaceRow_ = nullptr;
    QToolButton* caseButton_ = nullptr;
    QToolButton* wordButton_ = nullptr;
    QToolButton* regexButton_ = nullptr;
    QLabel* countLabel_ = nullptr;
};

} // namespace hungryeditor
