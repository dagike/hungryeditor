#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace hungryeditor {

/// One tab as recorded for the next launch: its file (empty for an untitled
/// buffer, which only returns through its recovery draft), the id of that
/// draft, and where the caret and viewport sat.
struct SessionDocument
{
    QString path;
    QString draftId;
    int caretLine = 0;
    int caretColumn = 0;
    int firstVisibleLine = 0;
};

/// The window as a clean shutdown left it: geometry, the active tab and the
/// open tabs in order.
struct Session
{
    bool valid = false; ///< false when no session file was found
    QByteArray windowGeometry;
    QByteArray windowState;   ///< QMainWindow::saveState — dock placement and visibility
    QByteArray splitterState; ///< QSplitter::saveState — the editor/preview split
    int currentIndex = 0;
    QString workspaceFolder; ///< explicitly opened folder, empty for none
    QString theme;           ///< Theme::builtinKey() of the active palette, empty for the default
    QString customThemePath; ///< a loaded custom theme file, empty when none is active
    QList<SessionDocument> documents;
};

/// Reads and writes the single session.json describing the last clean exit.
/// Its location is injected so the running app and the test suite never share
/// one.
class SessionStore
{
public:
    explicit SessionStore(QString filePath);

    QString filePath() const { return filePath_; }

    /// The recorded session, or an invalid one when the file is missing or
    /// unreadable.
    Session load() const;

    /// Write `session` atomically, creating the parent directory.
    bool save(const Session& session) const;

    /// Delete the session file — done once it has been consumed on startup.
    void clear() const;

private:
    QString filePath_;
};

} // namespace hungryeditor
