#pragma once

#include <functional>

#include <QFont>
#include <QString>
#include <QStringList>

#include "theme/Theme.h"

// Scintilla's headers are not self-contained and must be included in this
// order: ScintillaCall.h uses types from ScintillaTypes.h without including it.
// clang-format off
#include <ScintillaTypes.h>
#include <ScintillaCall.h>
#include <ScintillaEditBase.h>
// clang-format on

class QImage;
class QKeyEvent;

namespace Scintilla {
struct NotificationData;
}

namespace hungryeditor {

class Document;
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

    /// Number of active selections (carets).
    int selectionCount() const;
    /// The text of every active selection, in Scintilla's selection order.
    QStringList selectionTexts() const;
    /// The main selection's text (empty when the caret has no selection).
    QString selectedText() const;

    /// Sublime-style "select next occurrence": with no selection, select the
    /// word under the caret; with one, add the next case-sensitive match as an
    /// extra caret and make it the main selection, wrapping past end of file.
    void selectNextOccurrence();

    /// Rectangular (column) selection between two zero-based line/column
    /// coordinates — one caret per spanned line. The interactive paths are
    /// Alt+drag and Alt+Shift+arrows.
    void selectColumn(int anchorLine, int anchorColumn, int caretLine, int caretColumn);

    /// Whole-line editing operating on every line the selection touches.
    void moveLinesUp();
    void moveLinesDown();
    void duplicateSelection();
    void deleteLines();
    void joinLines();

    /// Toggle an HTML comment (`<!-- … -->`) on each selected line, after its
    /// indentation. This is Markdown's comment form; per-language comment
    /// tokens arrive with the grammar registry.
    void toggleLineComment();

    /// Markdown formatting shortcuts. Every operation is idempotent, acts on
    /// every active selection, and is a single undo step.

    /// Wrap each selection (or the word under a bare caret) in `marker`
    /// (`**`, `*`, `` ` ``, `~~`), or strip it when it is already there.
    void toggleInlineFormat(const QString& marker);
    /// Set the ATX heading level (1–6, or 0 to demote to a paragraph) on every
    /// line the selection touches, replacing any existing `#` prefix.
    void setHeadingLevel(int level);
    /// Advance the caret line's heading one level, wrapping `H6` back to a
    /// paragraph.
    void cycleHeading();
    /// Toggle a `> ` blockquote prefix on every selected line — added unless
    /// they all already have one, in which case it is removed.
    void toggleBlockquote();
    /// Toggle a `- ` bullet prefix on every selected line (same all-or-none
    /// rule as toggleBlockquote()).
    void toggleBulletList();
    /// Toggle an ordered-list prefix on every selected line, renumbering
    /// `1.`, `2.`, … down the block.
    void toggleNumberedList();
    /// Turn the selection into a Markdown link: a URL-looking selection becomes
    /// `[](url)` with the caret in the text slot, other text becomes
    /// `[text](url)` with `url` selected, and a bare caret inserts `[](url)`.
    void insertLink();

    /// Called with an image pulled from the clipboard on paste; returns the
    /// Markdown to insert in its place (e.g. `![](assets/x.png)`), or an empty
    /// string to fall back to Scintilla's normal paste. MainWindow installs one
    /// that writes the image to disk beside the document.
    using ImagePasteHandler = std::function<QString(const QImage&)>;
    void setImagePasteHandler(ImagePasteHandler handler);

    /// Smart paste: a single-line URL on the clipboard wraps the current
    /// selection as `[selection](url)`, and a clipboard image goes through the
    /// image handler. Returns true when it consumed the paste; false leaves it
    /// to Scintilla. Bound to Ctrl+V and Shift+Insert.
    bool handleSmartPaste();

    /// When the caret is inside a GFM pipe table, move it to the next (or
    /// previous) cell — realigning the whole table and appending a blank row
    /// when Tab steps past the last one — and select that cell's text. Returns
    /// false (for a plain tab / dedent) when the caret is not in a table.
    /// Bound to Tab and Shift+Tab.
    bool navigateTableCell(bool forward);

    /// Realign the pipe table under the caret in place. No-op otherwise.
    void formatTable();

    /// Set the task-list checkbox on `line` (`- [ ]` / `- [x]`, ordered markers
    /// included) to `checked`, as one undo step. No-op when the line carries no
    /// task marker or is already in that state. Driven by a click in the preview.
    void setTaskChecked(int line, bool checked);

    /// True when the document opens with a YAML front-matter block (`---` … `---`).
    bool hasFrontMatter() const;
    /// True when that block exists and is currently folded.
    bool isFrontMatterFolded() const;
    /// Fold or unfold the front-matter block. No-op when there is none, or when
    /// the caret sits inside it. Bound to Ctrl+Shift+Y via the View menu.
    void setFrontMatterFolded(bool folded);

    /// Position of the bracket that pairs with the one at `position`, or -1 if
    /// there is no bracket there or it is unbalanced.
    int matchingBrace(int position) const;

    /// How a find/replace matches.
    struct SearchOptions
    {
        bool matchCase = false;
        bool wholeWord = false;
        bool regex = false; ///< ECMAScript regex (std::regex-backed)
    };

    /// Select the next (or previous) match of `query` starting from the current
    /// selection, wrapping when `wrap`. Returns false if there is no match.
    bool findNext(const QString& query, const SearchOptions& options, bool forward = true,
                  bool wrap = true);

    /// If the current selection is a match of `query`, replace it (regex
    /// backreferences honoured) and select the following match; otherwise just
    /// advance to the next match. Returns whether a replacement was made.
    bool replaceCurrent(const QString& query, const QString& replacement,
                        const SearchOptions& options);

    /// Replace every match in the document in one undo step. Returns the count.
    int replaceAll(const QString& query, const QString& replacement, const SearchOptions& options);

    /// Outline every match of `query` with the find indicator (an empty query
    /// clears it). Returns the number of matches.
    int markAllMatches(const QString& query, const SearchOptions& options);

    /// Document line shown at the top of the viewport, zero-based. Persisted
    /// per tab so a switch or a restart returns to the same scroll offset.
    int firstVisibleLine() const;
    void setFirstVisibleLine(int line);

    /// Monospace font used for the text area. Changing it re-applies all
    /// derived styling (line-number margin width included).
    QFont editorFont() const { return font_; }
    void setEditorFont(const QFont& font);

    /// Spaces a Tab key press inserts (soft tabs; the buffer never holds "\t").
    int tabWidth() const { return tabWidth_; }
    void setTabWidth(int width);

    /// Whether long lines wrap at the viewport edge instead of scrolling.
    bool wordWrap() const { return wordWrap_; }
    void setWordWrap(bool wrap);

    /// Style byte at a position — for tests to check colouring.
    int styleAt(int position) const;

    /// Show `document` in the buffer. The previously attached document keeps
    /// its text, undo history and caret; this is an O(1) Scintilla pointer
    /// swap. DocumentManager owns the documents and calls this on a switch.
    void attachDocument(Document* document);
    Document* document() const { return document_; }

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
    /// Emitted when the vertical scroll position changes, for preview sync.
    void viewportScrolled();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void onNotify(Scintilla::NotificationData* notification);
    void applyHighlight(const HighlightResult& result);

    /// Highlight the bracket pair around the caret (or flag an unmatched one).
    void updateBraceHighlight();
    /// Shared engine for navigateTableCell()/formatTable(): realign the pipe
    /// table around the caret, optionally moving the caret one cell (`forward`
    /// direction) and selecting it. Returns false when the caret is not in a
    /// table.
    bool reflowTable(bool moveCaret, bool forward);

    /// Insert a newline that carries the current line's indentation and, if it
    /// is a Markdown list item, its (renumbered) marker — or, on an empty item,
    /// removes the marker instead. Returns false to fall back to a plain
    /// newline (multi-caret, selection, non-plain caret).
    bool insertSmartNewline();

    /// Apply fonts, colours, caret, tabs and margins from the current font
    /// and the (currently hard-coded) palette.
    void applyVisualDefaults();
    /// Configure the semantic Scintilla styles (fore colour, bold, italic).
    void applySyntaxStyles();
    /// Configure the styles used by Lexilla's stock Markdown lexer.
    void applyLexillaMarkdownStyles();
    /// Resize the line-number margin to fit the current line count.
    void updateLineNumberMargin();
    /// Re-derive the front-matter fold region from the buffer and show or hide
    /// the fold margin to match.
    void updateFrontMatterFold();
    /// Pick the highlighting tier for the current buffer size and, if it
    /// changed (or `force` is set), switch the lexer and the background
    /// highlighter to match and repaint.
    void updateHighlightTier(bool force = false);

    mutable Scintilla::ScintillaCall call_;
    HighlightController* highlight_ = nullptr;
    ImagePasteHandler imagePasteHandler_;
    Document* document_ = nullptr;
    QFont font_;
    int tabWidth_ = 4;
    bool wordWrap_ = false;
    Theme theme_ = Theme::builtin();
    bool modified_ = false;
    int lineDigits_ = 0;
    int frontMatterLastLine_ = -1; ///< closing `---` line, or -1 when absent
    HighlightTier tier_ = HighlightTier::TreeSitter;
    int lexillaByteLimit_ = 2 * 1024 * 1024;
    int plainTextByteLimit_ = 20 * 1024 * 1024;
};

} // namespace hungryeditor
