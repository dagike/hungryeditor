#pragma once

#include <QFrame>
#include <QList>
#include <QString>

class QKeyEvent;
class QListWidget;

namespace hungryeditor {

/// A transient overlay for Ctrl+Tab document switching: it lists the open
/// buffers in most-recently-used order, Tab steps down the list, and releasing
/// Ctrl commits the highlighted one. The window owns it, feeds it the entries
/// and acts on `accepted`.
class TabSwitcher : public QFrame
{
    Q_OBJECT

public:
    struct Entry
    {
        QString title;  ///< buffer display name
        QString detail; ///< directory or other hint, shown dimmed; may be empty
        bool modified = false;
    };

    explicit TabSwitcher(QWidget* parent = nullptr);

    /// Populate and reveal the overlay with `startRow` highlighted. No-op with
    /// an empty list.
    void present(const QList<Entry>& entries, int startRow);

    /// Whether the overlay is currently taking Ctrl+Tab input.
    bool isActive() const { return active_; }

    int currentRow() const;

    /// Move the highlight, wrapping at either end.
    void selectNext();
    void selectPrevious();

    /// Close the overlay, emitting `accepted` with the highlighted row.
    void commit();
    /// Close the overlay without choosing.
    void cancel();

signals:
    void accepted(int row);
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void resizeToContents();

    QListWidget* list_ = nullptr;
    bool active_ = false;
};

} // namespace hungryeditor
