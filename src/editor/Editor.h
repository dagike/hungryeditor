#pragma once

#include <QFont>
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

class HighlightController;
struct HighlightResult;

/// Thin, typed wrapper around Scintilla's editor widget.
///
/// It exposes the handful of operations the rest of the application needs as
/// ordinary C++ methods and translates Scintilla's `SCN_*` notifications into
/// Qt signals. Everything is UTF-8: Scintilla's buffer is configured for
/// code page 65001 and all `QString` conversions go through UTF-8.
///
/// Syntax colouring runs in container-lexing mode: a background
/// HighlightController parses the text with tree-sitter and hands back style
/// spans that are applied here. Large buffers step down to Lexilla's stock
/// Markdown lexer and then to plain text (see HighlightTier).
class Editor : public ScintillaEditBase
{
    Q_OBJECT

public:
    explicit Editor(QWidget* parent = nullptr);
    ~Editor() override;

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

    /// Monospace font used for the text area. Changing it re-applies all
    /// derived styling (line-number margin width included).
    QFont editorFont() const { return font_; }
    void setEditorFont(const QFont& font);

    /// Style byte at a position — for tests to check colouring.
    int styleAt(int position) const;

    /// How the buffer is being coloured. Large files drop from the
    /// tree-sitter highlighter to Lexilla's stock lexer and then to plain
    /// text, to keep editing responsive.
    enum class HighlightTier
    {
        TreeSitter,
        Lexilla,
        PlainText
    };
    HighlightTier highlightTier() const { return tier_; }

    /// Byte thresholds for the Lexilla and plain-text fallbacks. Exposed so
    /// tests can exercise the tiers without multi-megabyte fixtures.
    void setFallbackByteLimits(int lexillaLimit, int plainTextLimit);

    /// Escape hatch for code that needs the full Scintilla API.
    Scintilla::ScintillaCall& call() { return call_; }
    const Scintilla::ScintillaCall& call() const { return call_; }

signals:
    void textChanged();
    void modifiedChanged(bool modified);
    void cursorPositionChanged(int line, int column);
    /// Emitted after a background highlight pass has been applied.
    void highlightingApplied();

private:
    void onNotify(Scintilla::NotificationData* notification);
    void applyHighlight(const HighlightResult& result);

    /// Apply fonts, colours, caret, tabs and margins from the current font
    /// and the (currently hard-coded) palette.
    void applyVisualDefaults();
    /// Configure the semantic Scintilla styles (fore colour, bold, italic).
    void applySyntaxStyles();
    /// Configure the styles used by Lexilla's stock Markdown lexer.
    void applyLexillaMarkdownStyles();
    /// Resize the line-number margin to fit the current line count.
    void updateLineNumberMargin();
    /// Pick the highlighting tier for the current buffer size and, if it
    /// changed, switch the lexer and the background highlighter to match.
    void updateHighlightTier();

    mutable Scintilla::ScintillaCall call_;
    HighlightController* highlight_ = nullptr;
    QFont font_;
    bool modified_ = false;
    int lineDigits_ = 0;
    HighlightTier tier_ = HighlightTier::TreeSitter;
    int lexillaByteLimit_ = 2 * 1024 * 1024;
    int plainTextByteLimit_ = 20 * 1024 * 1024;
};

} // namespace hungryeditor
