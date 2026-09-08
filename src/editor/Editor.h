#pragma once

#include <QString>

// Scintilla's headers are not self-contained and must be included in this
// order: ScintillaCall.h uses types from ScintillaTypes.h without including it.
// clang-format off
#include <ScintillaTypes.h>
#include <ScintillaCall.h>
#include <ScintillaEditBase.h>
// clang-format on

namespace Scintilla {
struct NotificationData;
}

namespace hungryeditor {

/// Thin, typed wrapper around Scintilla's editor widget.
///
/// It exposes the handful of operations the rest of the application needs as
/// ordinary C++ methods and translates Scintilla's `SCN_*` notifications into
/// Qt signals. Everything is UTF-8: Scintilla's buffer is configured for
/// code page 65001 and all `QString` conversions go through UTF-8.
class Editor : public ScintillaEditBase
{
    Q_OBJECT

public:
    explicit Editor(QWidget* parent = nullptr);

    /// Whole-buffer contents.
    QString text() const;
    void setText(const QString& text);

    /// Buffer length in bytes (UTF-8), and number of lines.
    int length() const;
    int lineCount() const;

    /// True when the buffer has unsaved changes (Scintilla "save point").
    bool isModified() const;
    /// Mark the current state as saved; clears the modified flag.
    void markClean();

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    /// Caret position, zero-based.
    int cursorLine() const;
    int cursorColumn() const;
    void setCursorPosition(int line, int column);

    /// Escape hatch for code that needs the full Scintilla API.
    Scintilla::ScintillaCall& call() { return call_; }
    const Scintilla::ScintillaCall& call() const { return call_; }

signals:
    void textChanged();
    void modifiedChanged(bool modified);
    void cursorPositionChanged(int line, int column);

private:
    void onNotify(Scintilla::NotificationData* notification);

    mutable Scintilla::ScintillaCall call_;
    bool modified_ = false;
};

} // namespace hungryeditor
