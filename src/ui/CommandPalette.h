#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QLineEdit;
class QListWidget;

namespace hungryeditor {

/// A drop-down command palette: type to fuzzy-filter the window's actions,
/// Enter runs the highlighted one. The window owns it and feeds it commands.
class CommandPalette : public QWidget
{
    Q_OBJECT

public:
    struct Command
    {
        QString id;       ///< opaque handle passed back on accept
        QString title;    ///< shown text
        QString shortcut; ///< shown right-aligned, e.g. "Ctrl+P"
    };

    explicit CommandPalette(QWidget* parent = nullptr);

    void setCommands(const QList<Command>& commands);

    /// Reveal near the top of the parent, clear the query and take focus.
    void open();

signals:
    void commandChosen(const QString& id);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refilter();
    void accept();

    QLineEdit* query_ = nullptr;
    QListWidget* list_ = nullptr;
    QList<Command> commands_;
};

} // namespace hungryeditor
