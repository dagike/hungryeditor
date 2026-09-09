#include "editor/Editor.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <QColor>
#include <QFontDatabase>

#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>  // SCE_MARKDOWN_*
#include <Scintilla.h> // STYLE_DEFAULT / STYLE_LINENUMBER
#include <ScintillaMessages.h>
#include <ScintillaStructures.h>
#include <ScintillaTypes.h>
#include <tree_sitter/api.h>

#include "editor/Document.h"
#include "highlight/CaptureStyles.h"
#include "highlight/HighlightController.h"
#include "HighlightQueries.h" // generated: hungryeditor::queries::*

extern "C" const TSLanguage* tree_sitter_markdown(void);

namespace hungryeditor {

namespace {
using Scintilla::Message;

Scintilla::sptr_t sciSend(const ScintillaEditBase& editor, Message msg)
{
    // send() is const on ScintillaEditBase.
    return editor.send(static_cast<unsigned int>(msg));
}

/// Scintilla stores colours as 0x00BBGGRR integers.
Scintilla::Colour sciColour(const QColor& c)
{
    return c.red() | (c.green() << 8) | (c.blue() << 16);
}

/// Minimal light palette. The real, theme-driven palette arrives in Phase 3.
struct Palette
{
    QColor background{QStringLiteral("#ffffff")};
    QColor foreground{QStringLiteral("#1e1e1e")};
    QColor lineNumberText{QStringLiteral("#9aa0a6")};
    QColor lineNumberBackground{QStringLiteral("#f6f8fa")};
    QColor currentLine{QStringLiteral("#f2f6fc")};
    QColor selection{QStringLiteral("#cfe3ff")};
    QColor caret{QStringLiteral("#1e1e1e")};
    QColor findMatch{QStringLiteral("#f0b429")};
};

constexpr int kLineNumberMargin = 0;
constexpr int kSymbolMargin = 1;
constexpr int kFoldMargin = 2;
constexpr int kMinLineDigits = 3;
constexpr int kTabWidth = 4;
constexpr int kFindIndicator = 20; // in the user range (8..31)

Scintilla::FindOption searchFlags(const Editor::SearchOptions& options)
{
    using F = Scintilla::FindOption;
    F flags = F::None;
    if (options.matchCase) {
        flags |= F::MatchCase;
    }
    if (options.wholeWord) {
        flags |= F::WholeWord;
    }
    if (options.regex) {
        flags |= F::RegExp | F::Cxx11RegEx; // Cxx11RegEx must accompany RegExp
    }
    return flags;
}

/// Search `[from, to]` in the target (from > to searches backwards). On a hit
/// the caller reads call.Target{Start,End}(); returns whether anything matched.
bool searchRange(Scintilla::ScintillaCall& call, const QByteArray& needle, Scintilla::Position from,
                 Scintilla::Position to)
{
    call.SetTargetRange(from, to);
    return call.SearchInTarget(needle.size(), needle.constData()) >= 0;
}
} // namespace

Editor::Editor(QWidget* parent) : ScintillaEditBase(parent)
{
    // Scintilla hands back its direct-call entry point as an integer; casting
    // it to the function pointer type is how this API is meant to be used.
    const auto fn =
        reinterpret_cast<Scintilla::FunctionDirect>( // NOLINT(performance-no-int-to-ptr)
            sciSend(*this, Message::GetDirectStatusFunction));
    const auto ptr = sciSend(*this, Message::GetDirectPointer);
    call_.SetFnPtr(fn, ptr);

    call_.SetCodePage(static_cast<int>(Scintilla::CpUtf8));

    font_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (font_.pointSize() < 10) {
        font_.setPointSize(11);
    }
    applyVisualDefaults();

    connect(this, &ScintillaEditBase::notify, this, &Editor::onNotify);

    // Container-lexing highlighter, driven by a background tree-sitter parse.
    // The grammar is hard-coded to Markdown for now; per-document language
    // selection arrives with the grammar registry.
    highlight_ = new HighlightController(this);
    highlight_->configure(
        tree_sitter_markdown(),
        QString::fromUtf8(queries::kMarkdownHighlights.data(),
                          static_cast<qsizetype>(queries::kMarkdownHighlights.size())),
        QString::fromUtf8(queries::kMarkdownInjections.data(),
                          static_cast<qsizetype>(queries::kMarkdownInjections.size())));
    connect(highlight_, &HighlightController::highlighted, this, &Editor::applyHighlight);
    connect(this, &Editor::textChanged, this, [this] { highlight_->submit(text()); });

    updateHighlightTier();
}

Editor::~Editor() = default;

QString Editor::text() const
{
    const std::string s = call_.StringOfSpan({0, call_.TextLength()});
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

void Editor::setText(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    call_.SetText(utf8.constData());
    // The buffer is kept newline-only regardless of what the caller passed;
    // the real line ending lives on the Document (see Document::lineEnding()).
    call_.ConvertEOLs(Scintilla::EndOfLine::Lf);
}

int Editor::length() const
{
    return static_cast<int>(call_.TextLength());
}

int Editor::lineCount() const
{
    return static_cast<int>(call_.LineCount());
}

bool Editor::isModified() const
{
    return call_.Modify();
}

void Editor::markClean()
{
    call_.SetSavePoint();
}

void Editor::undo()
{
    call_.Undo();
}

void Editor::redo()
{
    call_.Redo();
}

bool Editor::canUndo() const
{
    return call_.CanUndo();
}

bool Editor::canRedo() const
{
    return call_.CanRedo();
}

int Editor::cursorLine() const
{
    return static_cast<int>(call_.LineFromPosition(call_.CurrentPos()));
}

int Editor::cursorColumn() const
{
    return static_cast<int>(call_.Column(call_.CurrentPos()));
}

void Editor::setCursorPosition(int line, int column)
{
    const Scintilla::Position pos = call_.FindColumn(line, column);
    call_.GotoPos(pos);
}

int Editor::selectionCount() const
{
    return static_cast<int>(call_.Selections());
}

QStringList Editor::selectionTexts() const
{
    QStringList texts;
    const int count = static_cast<int>(call_.Selections());
    for (int i = 0; i < count; ++i) {
        const std::string s =
            call_.StringOfSpan({call_.SelectionNStart(i), call_.SelectionNEnd(i)});
        texts << QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
    }
    return texts;
}

QString Editor::selectedText() const
{
    const std::string s = call_.StringOfSpan({call_.SelectionStart(), call_.SelectionEnd()});
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

void Editor::selectNextOccurrence()
{
    using Scintilla::Position;

    const auto main = static_cast<int>(call_.MainSelection());
    const Position start = call_.SelectionNStart(main);
    const Position end = call_.SelectionNEnd(main);

    if (start == end) {
        // No selection yet: take the word under the caret.
        const Position wordStart = call_.WordStartPosition(start, true);
        const Position wordEnd = call_.WordEndPosition(start, true);
        if (wordStart != wordEnd) {
            call_.SetSelection(wordEnd, wordStart); // caret, anchor
        }
        return;
    }

    const std::string phrase = call_.StringOfSpan({start, end});
    if (phrase.empty()) {
        return;
    }
    const auto length = static_cast<Position>(phrase.size());

    // Search from just past the furthest current selection, wrapping once.
    Position from = 0;
    const int count = static_cast<int>(call_.Selections());
    for (int i = 0; i < count; ++i) {
        from = std::max(from, call_.SelectionNEnd(i));
    }

    call_.SetSearchFlags(Scintilla::FindOption::MatchCase);
    const auto findIn = [&](Position lo, Position hi) {
        call_.SetTargetRange(lo, hi);
        return call_.SearchInTarget(length, phrase.c_str());
    };

    Position found = findIn(from, call_.TextLength());
    if (found < 0) {
        found = findIn(0, start); // wrap around to before the first match
    }
    if (found < 0) {
        return; // this is the only occurrence
    }
    for (int i = 0; i < count; ++i) {
        if (call_.SelectionNStart(i) == found) {
            return; // wrapped back onto a match already selected
        }
    }

    call_.AddSelection(found + length, found); // caret, anchor
    call_.SetMainSelection(static_cast<int>(call_.Selections()) - 1);
    call_.ScrollCaret();
}

void Editor::selectColumn(int anchorLine, int anchorColumn, int caretLine, int caretColumn)
{
    call_.SetRectangularSelectionAnchor(call_.FindColumn(anchorLine, anchorColumn));
    call_.SetRectangularSelectionCaret(call_.FindColumn(caretLine, caretColumn));
}

void Editor::moveLinesUp()
{
    call_.MoveSelectedLinesUp();
}

void Editor::moveLinesDown()
{
    call_.MoveSelectedLinesDown();
}

void Editor::duplicateSelection()
{
    if (call_.SelectionStart() == call_.SelectionEnd()) {
        call_.LineDuplicate();
    } else {
        call_.SelectionDuplicate();
    }
}

void Editor::deleteLines()
{
    const Scintilla::Line firstLine = call_.LineFromPosition(call_.SelectionStart());
    const Scintilla::Line lastLine = call_.LineFromPosition(call_.SelectionEnd());
    const Scintilla::Position from = call_.PositionFromLine(firstLine);
    const Scintilla::Position to = (lastLine + 1 >= call_.LineCount())
                                       ? call_.TextLength()
                                       : call_.PositionFromLine(lastLine + 1);
    call_.SetTargetRange(from, to);
    call_.ReplaceTarget(0, "");
}

void Editor::joinLines()
{
    using Line = Scintilla::Line;
    using Position = Scintilla::Position;

    const Line firstLine = call_.LineFromPosition(call_.SelectionStart());
    const Line lastLine = call_.LineFromPosition(call_.SelectionEnd());
    const Line throughLine = (lastLine > firstLine) ? lastLine : firstLine + 1;
    if (throughLine >= call_.LineCount()) {
        return;
    }

    call_.BeginUndoAction();
    // Bottom-up so earlier positions stay valid as text shrinks.
    for (Line line = throughLine; line > firstLine; --line) {
        const Position joinAt = call_.LineEndPosition(line - 1);
        Position nextContent = call_.PositionFromLine(line);
        const Position nextEnd = call_.LineEndPosition(line);
        while (nextContent < nextEnd) {
            const int ch = call_.CharAt(nextContent);
            if (ch != ' ' && ch != '\t') {
                break;
            }
            ++nextContent;
        }
        const bool addSpace = joinAt > call_.PositionFromLine(line - 1); // prev line had content
        call_.SetTargetRange(joinAt, nextContent);
        call_.ReplaceTarget(addSpace ? 1 : 0, addSpace ? " " : "");
    }
    call_.EndUndoAction();
}

namespace {

std::string trimmed(const std::string& s)
{
    const std::size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    return s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}

bool isHtmlComment(const std::string& body)
{
    const std::string t = trimmed(body);
    return t.size() >= 7 && t.rfind("<!--", 0) == 0 && t.compare(t.size() - 3, 3, "-->") == 0;
}

} // namespace

void Editor::toggleLineComment()
{
    const Scintilla::Line firstLine = call_.LineFromPosition(call_.SelectionStart());
    Scintilla::Line lastLine = call_.LineFromPosition(call_.SelectionEnd());
    if (lastLine > firstLine && call_.SelectionEnd() == call_.PositionFromLine(lastLine)) {
        --lastLine; // a selection ending at a line start doesn't include that line
    }

    const auto lineBody = [this](Scintilla::Line line) {
        return call_.StringOfSpan({call_.PositionFromLine(line), call_.LineEndPosition(line)});
    };

    bool addComments = false;
    for (Scintilla::Line line = firstLine; line <= lastLine; ++line) {
        const std::string body = lineBody(line);
        if (trimmed(body).empty()) {
            continue;
        }
        if (!isHtmlComment(body)) {
            addComments = true;
            break;
        }
    }

    call_.BeginUndoAction();
    for (Scintilla::Line line = firstLine; line <= lastLine; ++line) {
        const std::string body = lineBody(line);
        const std::string trimmedBody = trimmed(body);
        if (trimmedBody.empty()) {
            continue;
        }
        const std::size_t indentEnd = body.find_first_not_of(" \t");
        const std::string indent = body.substr(0, indentEnd);

        std::string replacement;
        if (addComments) {
            replacement = indent + "<!-- " + trimmedBody + " -->";
        } else if (isHtmlComment(body)) {
            std::string inner = trimmedBody.substr(4, trimmedBody.size() - 7);
            if (!inner.empty() && inner.front() == ' ') {
                inner.erase(0, 1);
            }
            if (!inner.empty() && inner.back() == ' ') {
                inner.pop_back();
            }
            replacement = indent + inner;
        } else {
            continue;
        }
        call_.SetTargetRange(call_.PositionFromLine(line), call_.LineEndPosition(line));
        call_.ReplaceTarget(Scintilla::Position(replacement.size()), replacement.c_str());
    }
    call_.EndUndoAction();
}

bool Editor::findNext(const QString& query, const SearchOptions& options, bool forward, bool wrap)
{
    if (query.isEmpty()) {
        return false;
    }
    const QByteArray needle = query.toUtf8();
    call_.SetSearchFlags(searchFlags(options));

    const Scintilla::Position docEnd = call_.TextLength();
    const Scintilla::Position selStart = call_.SelectionStart();
    const Scintilla::Position selEnd = call_.SelectionEnd();

    const bool hit = forward ? (searchRange(call_, needle, selEnd, docEnd) ||
                                (wrap && searchRange(call_, needle, 0, selStart)))
                             : (searchRange(call_, needle, selStart, 0) ||
                                (wrap && searchRange(call_, needle, docEnd, selEnd)));
    if (!hit) {
        return false;
    }
    call_.SetSelection(call_.TargetEnd(), call_.TargetStart()); // caret, anchor
    call_.ScrollCaret();
    return true;
}

bool Editor::replaceCurrent(const QString& query, const QString& replacement,
                            const SearchOptions& options)
{
    const Scintilla::Position start = call_.SelectionStart();
    const Scintilla::Position end = call_.SelectionEnd();
    const QByteArray needle = query.toUtf8();
    call_.SetSearchFlags(searchFlags(options));

    // Replace only if the selection is exactly a match; otherwise just advance.
    const bool selectionIsMatch = start != end && searchRange(call_, needle, start, end) &&
                                  call_.TargetStart() == start && call_.TargetEnd() == end;
    if (!selectionIsMatch) {
        return findNext(query, options, /*forward=*/true, /*wrap=*/true);
    }

    const QByteArray repl = replacement.toUtf8();
    const Scintilla::Position newLength = options.regex
                                              ? call_.ReplaceTargetRE(repl.size(), repl.constData())
                                              : call_.ReplaceTarget(repl.size(), repl.constData());
    call_.SetSelection(start + newLength, start); // caret, anchor
    findNext(query, options, /*forward=*/true, /*wrap=*/true);
    return true;
}

int Editor::replaceAll(const QString& query, const QString& replacement,
                       const SearchOptions& options)
{
    if (query.isEmpty()) {
        return 0;
    }
    const QByteArray needle = query.toUtf8();
    const QByteArray repl = replacement.toUtf8();
    call_.SetSearchFlags(searchFlags(options));

    int replaced = 0;
    call_.BeginUndoAction();
    Scintilla::Position from = 0;
    while (searchRange(call_, needle, from, call_.TextLength())) {
        const Scintilla::Position matchStart = call_.TargetStart();
        const Scintilla::Position matchEnd = call_.TargetEnd();
        const Scintilla::Position newLength =
            options.regex ? call_.ReplaceTargetRE(repl.size(), repl.constData())
                          : call_.ReplaceTarget(repl.size(), repl.constData());
        from = matchStart + newLength + (matchEnd == matchStart ? 1 : 0);
        ++replaced;
    }
    call_.EndUndoAction();
    return replaced;
}

int Editor::markAllMatches(const QString& query, const SearchOptions& options)
{
    call_.SetIndicatorCurrent(kFindIndicator);
    call_.IndicatorClearRange(0, call_.TextLength());
    if (query.isEmpty()) {
        return 0;
    }
    const QByteArray needle = query.toUtf8();
    call_.SetSearchFlags(searchFlags(options));

    int matches = 0;
    Scintilla::Position from = 0;
    while (searchRange(call_, needle, from, call_.TextLength())) {
        const Scintilla::Position matchStart = call_.TargetStart();
        const Scintilla::Position matchEnd = call_.TargetEnd();
        call_.IndicatorFillRange(matchStart, matchEnd - matchStart);
        from = matchEnd + (matchEnd == matchStart ? 1 : 0);
        ++matches;
    }
    return matches;
}

int Editor::firstVisibleLine() const
{
    return static_cast<int>(call_.FirstVisibleLine());
}

void Editor::setFirstVisibleLine(int line)
{
    call_.SetFirstVisibleLine(line);
    // Scintilla only sends SC_UPDATE_V_SCROLL when it repaints, which a hidden
    // or idle widget will not do — surface programmatic scrolls directly.
    emit viewportScrolled();
}

void Editor::setEditorFont(const QFont& font)
{
    font_ = font;
    applyVisualDefaults();
}

void Editor::attachDocument(Document* document)
{
    document_ = document;
    call_.SetDocPointer(document != nullptr ? document->pointer() : nullptr);

    const bool nowModified = call_.Modify();
    if (nowModified != modified_) {
        modified_ = nowModified;
        emit modifiedChanged(modified_);
    }

    // Style definitions are per-view and survive the swap; the style bytes and
    // buffer size are per-document, so re-pick the size tier and repaint the
    // newly-visible text unconditionally.
    updateHighlightTier(/*force=*/true);
    lineDigits_ = 0;
    updateLineNumberMargin();
}

void Editor::applyVisualDefaults()
{
    const Palette palette;
    const QByteArray family = font_.family().toUtf8();
    const int pointSize = std::max(font_.pointSize(), 6);

    // Base text style, then propagate it to every other style.
    call_.StyleSetFont(STYLE_DEFAULT, family.constData());
    call_.StyleSetSize(STYLE_DEFAULT, pointSize);
    call_.StyleSetFore(STYLE_DEFAULT, sciColour(palette.foreground));
    call_.StyleSetBack(STYLE_DEFAULT, sciColour(palette.background));
    call_.StyleClearAll();

    call_.StyleSetFore(STYLE_LINENUMBER, sciColour(palette.lineNumberText));
    call_.StyleSetBack(STYLE_LINENUMBER, sciColour(palette.lineNumberBackground));

    call_.SetElementColour(Scintilla::Element::Caret, sciColour(palette.caret));
    call_.SetSelBack(true, sciColour(palette.selection));
    call_.SetCaretLineVisible(true);
    call_.SetCaretLineBack(sciColour(palette.currentLine));
    call_.SetCaretWidth(2);
    call_.SetCaretPeriod(500);

    call_.SetEOLMode(Scintilla::EndOfLine::Lf);
    call_.SetTabWidth(kTabWidth);
    call_.SetUseTabs(false);
    call_.SetViewWS(Scintilla::WhiteSpace::Invisible);
    call_.SetWrapMode(Scintilla::Wrap::None);
    call_.SetScrollWidthTracking(true);
    call_.SetScrollWidth(1);

    // Multi-caret editing: Ctrl+click adds carets, Alt+drag selects a column,
    // and typing/pasting acts on every selection.
    call_.SetMultipleSelection(true);
    call_.SetAdditionalSelectionTyping(true);
    call_.SetMultiPaste(Scintilla::MultiPaste::Each);

    // Rectangular (column) selection: Alt+drag, or hold Alt to switch a normal
    // drag into a rectangular one. Alt+Shift+arrows is Scintilla's keyboard path.
    call_.SetRectangularSelectionModifier(static_cast<int>(Scintilla::KeyMod::Alt));
    call_.SetMouseSelectionRectangularSwitch(true);
    call_.SetAdditionalCaretsBlink(true);
    call_.SetAdditionalCaretFore(sciColour(palette.caret));
    call_.SetElementColour(Scintilla::Element::SelectionAdditionalBack,
                           sciColour(palette.selection));

    // Find bar: outline every match while the bar is open.
    call_.IndicSetStyle(kFindIndicator, Scintilla::IndicatorStyle::StraightBox);
    call_.IndicSetFore(kFindIndicator, sciColour(palette.findMatch));
    call_.IndicSetAlpha(kFindIndicator, static_cast<Scintilla::Alpha>(70));
    call_.IndicSetOutlineAlpha(kFindIndicator, static_cast<Scintilla::Alpha>(160));

    call_.SetMarginTypeN(kLineNumberMargin, Scintilla::MarginType::Number);
    call_.SetMarginWidthN(kSymbolMargin, 0);
    call_.SetMarginWidthN(kFoldMargin, 0);

    applySyntaxStyles();

    lineDigits_ = 0; // force updateLineNumberMargin() to recompute
    updateLineNumberMargin();
}

void Editor::applySyntaxStyles()
{
    // Must run after StyleClearAll(), which resets every style to the default.
    for (const StyleDef& def : styleTable()) {
        if (def.id == StylePlain) {
            continue;
        }
        call_.StyleSetFore(def.id, sciColour(def.foreground));
        call_.StyleSetBold(def.id, def.bold);
        call_.StyleSetItalic(def.id, def.italic);
        call_.StyleSetUnderline(def.id, def.underline);
    }
}

void Editor::applyLexillaMarkdownStyles()
{
    // Lexilla's Markdown lexer owns style ids 0..21 (SCE_MARKDOWN_*), which
    // overlap the semantic ids used in tree-sitter mode — so the styles are
    // re-declared on every switch into and out of this tier.
    const Palette palette;
    const auto heading = sciColour(QColor(QStringLiteral("#0550ae")));
    const auto code = sciColour(QColor(QStringLiteral("#6e40c9")));
    const auto marker = sciColour(QColor(QStringLiteral("#57606a")));

    call_.StyleClearAll();
    for (int header = SCE_MARKDOWN_HEADER1; header <= SCE_MARKDOWN_HEADER6; ++header) {
        call_.StyleSetFore(header, heading);
        call_.StyleSetBold(header, true);
    }
    call_.StyleSetBold(SCE_MARKDOWN_STRONG1, true);
    call_.StyleSetBold(SCE_MARKDOWN_STRONG2, true);
    call_.StyleSetItalic(SCE_MARKDOWN_EM1, true);
    call_.StyleSetItalic(SCE_MARKDOWN_EM2, true);
    call_.StyleSetFore(SCE_MARKDOWN_STRIKEOUT, marker);
    call_.StyleSetFore(SCE_MARKDOWN_PRECHAR, marker);
    call_.StyleSetFore(SCE_MARKDOWN_ULIST_ITEM, marker);
    call_.StyleSetFore(SCE_MARKDOWN_OLIST_ITEM, marker);
    call_.StyleSetFore(SCE_MARKDOWN_BLOCKQUOTE, marker);
    call_.StyleSetFore(SCE_MARKDOWN_HRULE, marker);
    call_.StyleSetFore(SCE_MARKDOWN_LINK, sciColour(QColor(QStringLiteral("#0969da"))));
    call_.StyleSetFore(SCE_MARKDOWN_CODE, code);
    call_.StyleSetFore(SCE_MARKDOWN_CODE2, code);
    call_.StyleSetFore(SCE_MARKDOWN_CODEBK, code);

    call_.StyleSetFore(STYLE_LINENUMBER, sciColour(palette.lineNumberText));
    call_.StyleSetBack(STYLE_LINENUMBER, sciColour(palette.lineNumberBackground));
}

void Editor::updateHighlightTier(bool force)
{
    const int bytes = length();
    HighlightTier wanted = HighlightTier::TreeSitter;
    if (bytes > plainTextByteLimit_) {
        wanted = HighlightTier::PlainText;
    } else if (bytes > lexillaByteLimit_) {
        wanted = HighlightTier::Lexilla;
    }
    if (wanted == tier_ && !force) {
        return;
    }
    tier_ = wanted;

    switch (tier_) {
    case HighlightTier::TreeSitter:
        call_.SetILexer(nullptr);
        applyVisualDefaults(); // restores the semantic style table
        highlight_->setEnabled(true);
        highlight_->submit(text());
        break;

    case HighlightTier::Lexilla: {
        highlight_->setEnabled(false);
        Scintilla::ILexer5* lexer = CreateLexer("markdown");
        call_.SetILexer(lexer); // Scintilla releases it on the next SetILexer
        applyLexillaMarkdownStyles();
        call_.Colourise(0, -1);
        break;
    }

    case HighlightTier::PlainText:
        highlight_->setEnabled(false);
        call_.SetILexer(nullptr);
        applyVisualDefaults();
        call_.StartStyling(0, 0);
        call_.SetStyling(call_.TextLength(), StylePlain);
        break;
    }
}

void Editor::setFallbackByteLimits(int lexillaLimit, int plainTextLimit)
{
    lexillaByteLimit_ = lexillaLimit;
    plainTextByteLimit_ = plainTextLimit;
    updateHighlightTier();
}

void Editor::applyHighlight(const HighlightResult& result)
{
    if (!result.ok || tier_ != HighlightTier::TreeSitter) {
        return;
    }
    const auto docLength = static_cast<quint32>(call_.TextLength());

    call_.StartStyling(0, 0);
    quint32 styled = 0;
    for (const HighlightSpan& span : result.spans) {
        if (span.start != styled || styled >= docLength) {
            break; // document changed under us; the next parse will catch up
        }
        const quint32 length = std::min(span.length, docLength - styled);
        call_.SetStyling(length, span.style);
        styled += length;
    }
    if (styled < docLength) {
        call_.SetStyling(docLength - styled, StylePlain);
    }
    emit highlightingApplied();
}

void Editor::updateLineNumberMargin()
{
    int digits = 1;
    for (int lines = lineCount(); lines >= 10; lines /= 10) {
        ++digits;
    }
    digits = std::max(digits, kMinLineDigits);
    if (digits == lineDigits_) {
        return;
    }
    lineDigits_ = digits;

    const std::string sample(static_cast<std::size_t>(digits) + 1, '9');
    const int width = call_.TextWidth(STYLE_LINENUMBER, sample.c_str());
    call_.SetMarginWidthN(kLineNumberMargin, width);
}

void Editor::onNotify(Scintilla::NotificationData* notification)
{
    using Scintilla::FlagSet;
    using Scintilla::ModificationFlags;
    using Scintilla::Notification;
    using Scintilla::Update;

    switch (notification->nmhdr.code) {
    case Notification::Modified:
        if (FlagSet(notification->modificationType,
                    ModificationFlags::InsertText | ModificationFlags::DeleteText)) {
            if (notification->linesAdded != 0) {
                updateLineNumberMargin();
            }
            updateHighlightTier();
            emit textChanged();
        }
        break;

    case Notification::SavePointReached:
        if (modified_) {
            modified_ = false;
            emit modifiedChanged(false);
        }
        break;

    case Notification::SavePointLeft:
        if (!modified_) {
            modified_ = true;
            emit modifiedChanged(true);
        }
        break;

    case Notification::UpdateUI:
        if (FlagSet(notification->updated, Update::Selection)) {
            emit cursorPositionChanged(cursorLine(), cursorColumn());
        }
        if (FlagSet(notification->updated, Update::VScroll)) {
            emit viewportScrolled();
        }
        break;

    case Notification::StyleNeeded: {
        // Container-lexing contract: fill the gap Scintilla asks about so it
        // stops requesting. The real colours arrive from applyHighlight()
        // once the background parse for this revision finishes.
        const Scintilla::Position from = call_.EndStyled();
        const Scintilla::Position to = notification->position;
        if (to > from) {
            call_.StartStyling(from, 0);
            call_.SetStyling(to - from, StylePlain);
        }
        break;
    }

    default:
        break;
    }
}

int Editor::styleAt(int position) const
{
    return static_cast<int>(call_.UnsignedStyleAt(position));
}

} // namespace hungryeditor
