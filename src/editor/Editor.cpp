#include "editor/Editor.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QClipboard>
#include <QColor>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>

#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>  // SCE_MARKDOWN_*
#include <Scintilla.h> // STYLE_DEFAULT / STYLE_LINENUMBER
#include <ScintillaMessages.h>
#include <ScintillaStructures.h>
#include <ScintillaTypes.h>
#include <tree_sitter/api.h>

#include "editor/Document.h"
#include "editor/MarkdownTable.h"
#include "highlight/CaptureStyles.h"
#include "highlight/HighlightController.h"
#include "HighlightQueries.h" // generated: hungryeditor::queries::*
#include "markdown/FrontMatter.h"

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
    QColor braceMatch{QStringLiteral("#bfe3c6")};
    QColor braceBad{QStringLiteral("#cf222e")};
};

constexpr int kLineNumberMargin = 0;
constexpr int kSymbolMargin = 1;
constexpr int kFoldMargin = 2;
constexpr int kMinLineDigits = 3;
constexpr int kFindIndicator = 20; // in the user range (8..31)
constexpr int kFoldMarginWidth = 14;

/// Build a Scintilla fold level: `SC_FOLDLEVELBASE + number`, with the header
/// flag when `header` is set. The enum has no `operator|`.
Scintilla::FoldLevel foldLevel(int number, bool header)
{
    int bits = static_cast<int>(Scintilla::FoldLevel::Base) + number;
    if (header) {
        bits |= static_cast<int>(Scintilla::FoldLevel::HeaderFlag);
    }
    return static_cast<Scintilla::FoldLevel>(bits);
}

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

/// Whether `text` reads as a bare URL we can drop into a link target.
bool looksLikeUrl(std::string_view text)
{
    return text.rfind("http://", 0) == 0 || text.rfind("https://", 0) == 0 ||
           text.rfind("www.", 0) == 0 || text.rfind("mailto:", 0) == 0;
}

/// The inclusive range of lines the current selection touches. A selection
/// ending exactly at a line's start does not pull that line in.
std::pair<Scintilla::Line, Scintilla::Line> selectedLineSpan(Scintilla::ScintillaCall& call)
{
    const Scintilla::Line first = call.LineFromPosition(call.SelectionStart());
    Scintilla::Line last = call.LineFromPosition(call.SelectionEnd());
    if (last > first && call.SelectionEnd() == call.PositionFromLine(last)) {
        --last;
    }
    return {first, last};
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

        std::string replacement = indent;
        if (addComments) {
            replacement += "<!-- ";
            replacement += trimmedBody;
            replacement += " -->";
        } else if (isHtmlComment(body)) {
            std::string inner = trimmedBody.substr(4, trimmedBody.size() - 7);
            if (!inner.empty() && inner.front() == ' ') {
                inner.erase(0, 1);
            }
            if (!inner.empty() && inner.back() == ' ') {
                inner.pop_back();
            }
            replacement += inner;
        } else {
            continue;
        }
        call_.SetTargetRange(call_.PositionFromLine(line), call_.LineEndPosition(line));
        call_.ReplaceTarget(Scintilla::Position(replacement.size()), replacement.c_str());
    }
    call_.EndUndoAction();
}

void Editor::toggleInlineFormat(const QString& marker)
{
    using Position = Scintilla::Position;

    const std::string mk = marker.toStdString();
    if (mk.empty()) {
        return;
    }
    const auto mkLen = static_cast<Position>(mk.size());

    // Resolve every selection to a span up front (a bare caret takes the word
    // under it), then edit bottom-up so earlier positions stay valid.
    struct Span
    {
        Position start;
        Position end;
    };
    std::vector<Span> spans;
    const int count = static_cast<int>(call_.Selections());
    spans.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        Position s = call_.SelectionNStart(i);
        Position e = call_.SelectionNEnd(i);
        if (s == e) {
            s = call_.WordStartPosition(s, true);
            e = call_.WordEndPosition(e, true);
        }
        spans.push_back({s, e});
    }
    std::sort(spans.begin(), spans.end(),
              [](const Span& a, const Span& b) { return a.start > b.start; });

    call_.BeginUndoAction();
    bool firstSpan = true;
    for (const Span& span : spans) {
        const Position s = span.start;
        const Position e = span.end;
        const std::string before = call_.StringOfSpan({std::max<Position>(0, s - mkLen), s});
        const std::string after = call_.StringOfSpan({e, std::min(call_.TextLength(), e + mkLen)});
        const std::string inner = call_.StringOfSpan({s, e});

        Position selStart = 0;
        Position selEnd = 0;
        if (before == mk && after == mk) {
            call_.SetTargetRange(e, e + mkLen);
            call_.ReplaceTarget(0, "");
            call_.SetTargetRange(s - mkLen, s);
            call_.ReplaceTarget(0, "");
            selStart = s - mkLen;
            selEnd = e - mkLen;
        } else if (static_cast<Position>(inner.size()) >= 2 * mkLen &&
                   inner.compare(0, mk.size(), mk) == 0 &&
                   inner.compare(inner.size() - mk.size(), mk.size(), mk) == 0) {
            call_.SetTargetRange(e - mkLen, e);
            call_.ReplaceTarget(0, "");
            call_.SetTargetRange(s, s + mkLen);
            call_.ReplaceTarget(0, "");
            selStart = s;
            selEnd = e - 2 * mkLen;
        } else {
            call_.SetTargetRange(e, e);
            call_.ReplaceTarget(mkLen, mk.c_str());
            call_.SetTargetRange(s, s);
            call_.ReplaceTarget(mkLen, mk.c_str());
            selStart = s + mkLen;
            selEnd = e + mkLen;
        }

        if (firstSpan) {
            call_.SetSelection(selEnd, selStart); // caret, anchor
            firstSpan = false;
        } else {
            call_.AddSelection(selEnd, selStart);
        }
    }
    call_.EndUndoAction();
}

namespace {

/// Length of a leading ATX heading marker (`#`…`###### ` then whitespace), or 0
/// when the line does not open with one.
std::size_t headingMarkerLength(const std::string& body)
{
    std::size_t hashes = 0;
    while (hashes < body.size() && body[hashes] == '#') {
        ++hashes;
    }
    if (hashes < 1 || hashes > 6 || hashes >= body.size()) {
        return 0;
    }
    if (body[hashes] != ' ' && body[hashes] != '\t') {
        return 0;
    }
    std::size_t end = hashes;
    while (end < body.size() && (body[end] == ' ' || body[end] == '\t')) {
        ++end;
    }
    return end;
}

} // namespace

void Editor::setHeadingLevel(int level)
{
    level = std::clamp(level, 0, 6);
    const auto [firstLine, lastLine] = selectedLineSpan(call_);

    call_.BeginUndoAction();
    for (Scintilla::Line line = lastLine; line >= firstLine; --line) {
        const Scintilla::Position lineStart = call_.PositionFromLine(line);
        const std::string body = call_.StringOfSpan({lineStart, call_.LineEndPosition(line)});
        const std::string content = body.substr(headingMarkerLength(body));

        std::string replacement;
        if (level > 0) {
            replacement.assign(static_cast<std::size_t>(level), '#');
            replacement += ' ';
        }
        replacement += content;
        call_.SetTargetRange(lineStart, call_.LineEndPosition(line));
        call_.ReplaceTarget(Scintilla::Position(replacement.size()), replacement.c_str());
    }
    call_.EndUndoAction();
}

void Editor::cycleHeading()
{
    const Scintilla::Line line = call_.LineFromPosition(call_.SelectionStart());
    const std::string body =
        call_.StringOfSpan({call_.PositionFromLine(line), call_.LineEndPosition(line)});
    std::size_t hashes = 0;
    while (hashes < body.size() && body[hashes] == '#') {
        ++hashes;
    }
    const int current = headingMarkerLength(body) > 0 ? static_cast<int>(hashes) : 0;
    setHeadingLevel(current >= 6 ? 0 : current + 1);
}

namespace {

/// Offset of the first non-blank character, or npos for a blank line.
std::size_t indentEnd(const std::string& body)
{
    return body.find_first_not_of(" \t");
}

bool isBulletMarker(const std::string& body, std::size_t at)
{
    return at + 1 < body.size() && (body[at] == '-' || body[at] == '*' || body[at] == '+') &&
           body[at + 1] == ' ';
}

/// Length of a leading ordered-list marker (`12. ` / `3) `) at `at`, or 0.
std::size_t orderedMarkerLength(const std::string& body, std::size_t at)
{
    std::size_t digits = at;
    while (digits < body.size() && std::isdigit(static_cast<unsigned char>(body[digits])) != 0) {
        ++digits;
    }
    if (digits == at || digits + 1 >= body.size()) {
        return 0;
    }
    if ((body[digits] != '.' && body[digits] != ')') || body[digits + 1] != ' ') {
        return 0;
    }
    return digits + 2 - at;
}

} // namespace

void Editor::toggleBlockquote()
{
    const auto [firstLine, lastLine] = selectedLineSpan(call_);

    bool add = false;
    for (Scintilla::Line line = firstLine; line <= lastLine; ++line) {
        const std::string body =
            call_.StringOfSpan({call_.PositionFromLine(line), call_.LineEndPosition(line)});
        const std::size_t c = indentEnd(body);
        if (c != std::string::npos && body[c] != '>') {
            add = true;
            break;
        }
    }

    call_.BeginUndoAction();
    for (Scintilla::Line line = lastLine; line >= firstLine; --line) {
        const Scintilla::Position lineStart = call_.PositionFromLine(line);
        const std::string body = call_.StringOfSpan({lineStart, call_.LineEndPosition(line)});
        const std::size_t c = indentEnd(body);

        std::string replacement;
        if (add) {
            const std::size_t at = (c == std::string::npos) ? body.size() : c;
            replacement = body.substr(0, at);
            replacement += "> ";
            replacement += body.substr(at);
        } else {
            if (c == std::string::npos || body[c] != '>') {
                continue;
            }
            std::size_t rest = c + 1;
            if (rest < body.size() && body[rest] == ' ') {
                ++rest;
            }
            replacement = body.substr(0, c);
            replacement += body.substr(rest);
        }
        call_.SetTargetRange(lineStart, call_.LineEndPosition(line));
        call_.ReplaceTarget(Scintilla::Position(replacement.size()), replacement.c_str());
    }
    call_.EndUndoAction();
}

void Editor::toggleBulletList()
{
    const auto [firstLine, lastLine] = selectedLineSpan(call_);

    bool add = false;
    for (Scintilla::Line line = firstLine; line <= lastLine; ++line) {
        const std::string body =
            call_.StringOfSpan({call_.PositionFromLine(line), call_.LineEndPosition(line)});
        const std::size_t c = indentEnd(body);
        if (c != std::string::npos && !isBulletMarker(body, c)) {
            add = true;
            break;
        }
    }

    call_.BeginUndoAction();
    for (Scintilla::Line line = lastLine; line >= firstLine; --line) {
        const Scintilla::Position lineStart = call_.PositionFromLine(line);
        const std::string body = call_.StringOfSpan({lineStart, call_.LineEndPosition(line)});
        const std::size_t c = indentEnd(body);
        if (c == std::string::npos) {
            continue;
        }

        std::string replacement = body.substr(0, c);
        if (add) {
            replacement += "- ";
            replacement += body.substr(c);
        } else {
            if (!isBulletMarker(body, c)) {
                continue;
            }
            replacement += body.substr(c + 2);
        }
        call_.SetTargetRange(lineStart, call_.LineEndPosition(line));
        call_.ReplaceTarget(Scintilla::Position(replacement.size()), replacement.c_str());
    }
    call_.EndUndoAction();
}

void Editor::toggleNumberedList()
{
    const auto [firstLine, lastLine] = selectedLineSpan(call_);

    bool add = false;
    for (Scintilla::Line line = firstLine; line <= lastLine; ++line) {
        const std::string body =
            call_.StringOfSpan({call_.PositionFromLine(line), call_.LineEndPosition(line)});
        const std::size_t c = indentEnd(body);
        if (c != std::string::npos && orderedMarkerLength(body, c) == 0) {
            add = true;
            break;
        }
    }

    call_.BeginUndoAction();
    long ordinal = 1;
    for (Scintilla::Line line = firstLine; line <= lastLine; ++line) {
        const Scintilla::Position lineStart = call_.PositionFromLine(line);
        const std::string body = call_.StringOfSpan({lineStart, call_.LineEndPosition(line)});
        const std::size_t c = indentEnd(body);
        if (c == std::string::npos) {
            continue;
        }

        std::string replacement = body.substr(0, c);
        if (add) {
            replacement += std::to_string(ordinal);
            replacement += ". ";
            replacement += body.substr(c);
            ++ordinal;
        } else {
            const std::size_t markerLen = orderedMarkerLength(body, c);
            if (markerLen == 0) {
                continue;
            }
            replacement += body.substr(c + markerLen);
        }
        call_.SetTargetRange(lineStart, call_.LineEndPosition(line));
        call_.ReplaceTarget(Scintilla::Position(replacement.size()), replacement.c_str());
    }
    call_.EndUndoAction();
}

void Editor::insertLink()
{
    using Position = Scintilla::Position;

    const Position s = call_.SelectionStart();
    const Position e = call_.SelectionEnd();
    const std::string sel = call_.StringOfSpan({s, e});

    call_.BeginUndoAction();
    if (s != e && looksLikeUrl(sel)) {
        std::string repl = "[](";
        repl += sel;
        repl += ")";
        call_.SetTargetRange(s, e);
        call_.ReplaceTarget(Position(repl.size()), repl.c_str());
        call_.SetSelection(s + 1, s + 1); // caret between the brackets
    } else if (s != e) {
        std::string repl = "[";
        repl += sel;
        repl += "](url)";
        call_.SetTargetRange(s, e);
        call_.ReplaceTarget(Position(repl.size()), repl.c_str());
        const Position urlStart = s + 1 + static_cast<Position>(sel.size()) + 2;
        call_.SetSelection(urlStart + 3, urlStart); // select "url"
    } else {
        call_.ReplaceSel("[](url)");
        call_.SetSelection(s + 1, s + 1); // caret between the brackets
    }
    call_.EndUndoAction();
}

namespace {

int unescapedPipesBefore(const std::string& text)
{
    int pipes = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\') {
            ++i;
            continue;
        }
        if (text[i] == '|') {
            ++pipes;
        }
    }
    return pipes;
}

/// Byte offsets [start, end) of column `col`'s trimmed content within a
/// rendered table line (which always begins and ends with a pipe).
std::pair<int, int> cellContentSpan(const std::string& line, int col)
{
    int seen = 0;
    int startPipe = -1;
    int endPipe = -1;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\') {
            ++i;
            continue;
        }
        if (line[i] != '|') {
            continue;
        }
        if (seen == col) {
            startPipe = static_cast<int>(i);
        } else if (seen == col + 1) {
            endPipe = static_cast<int>(i);
            break;
        }
        ++seen;
    }
    if (startPipe < 0) {
        return {static_cast<int>(line.size()), static_cast<int>(line.size())};
    }
    auto start = static_cast<std::size_t>(startPipe) + 1;
    auto end = endPipe < 0 ? line.size() : static_cast<std::size_t>(endPipe);
    while (start < end && line[start] == ' ') {
        ++start;
    }
    while (end > start && line[end - 1] == ' ') {
        --end;
    }
    return {static_cast<int>(start), static_cast<int>(end)};
}

} // namespace

bool Editor::reflowTable(bool moveCaret, bool forward)
{
    if (call_.Selections() != 1) {
        return false;
    }

    const Scintilla::Position caret = call_.CurrentPos();
    const int caretLine = static_cast<int>(call_.LineFromPosition(caret));

    const QStringList qlines = text().split(QLatin1Char('\n'));
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(qlines.size()));
    for (const QString& line : qlines) {
        lines.push_back(line.toStdString());
    }
    const auto lineAt = [&](int i) -> const std::string& {
        return lines[static_cast<std::size_t>(i)];
    };

    const mdtable::TableRegion region = mdtable::findTableRegion(lines, caretLine);
    if (!region.valid) {
        return false;
    }

    const std::vector<mdtable::ColumnAlign> aligns =
        mdtable::parseAlignments(lineAt(region.firstLine + 1));

    // Content rows: the header plus every row below the delimiter.
    std::vector<std::vector<std::string>> grid;
    grid.push_back(mdtable::splitCells(lineAt(region.firstLine)));
    for (int l = region.firstLine + 2; l <= region.lastLine; ++l) {
        grid.push_back(mdtable::splitCells(lineAt(l)));
    }

    int columns = static_cast<int>(aligns.size());
    for (const std::vector<std::string>& row : grid) {
        columns = std::max(columns, static_cast<int>(row.size()));
    }

    // Which logical cell the caret sits in.
    const std::string before = call_.StringOfSpan({call_.PositionFromLine(caretLine), caret});
    const std::string& caretText = lineAt(caretLine);
    const std::size_t firstNonSpace = caretText.find_first_not_of(" \t");
    const bool leadingPipe = firstNonSpace != std::string::npos && caretText[firstNonSpace] == '|';
    int col = std::clamp(unescapedPipesBefore(before) - (leadingPipe ? 1 : 0), 0, columns - 1);
    int row = (caretLine <= region.firstLine + 1) ? 0 : caretLine - region.firstLine - 1;

    if (moveCaret && forward) {
        if (++col >= columns) {
            col = 0;
            ++row;
        }
        if (row >= static_cast<int>(grid.size())) {
            grid.emplace_back();
        }
    } else if (moveCaret) {
        if (--col < 0) {
            if (--row < 0) {
                row = 0;
                col = 0;
            } else {
                col = columns - 1;
            }
        }
    }

    const std::string rendered = mdtable::renderAligned(grid, aligns);
    const Scintilla::Position regionStart = call_.PositionFromLine(region.firstLine);
    const Scintilla::Position regionEnd = call_.LineEndPosition(region.lastLine);

    call_.BeginUndoAction();
    call_.SetTargetRange(regionStart, regionEnd);
    call_.ReplaceTarget(static_cast<Scintilla::Position>(rendered.size()), rendered.c_str());

    // Locate the target cell in the freshly rendered text (row 0 is the header,
    // row 1 the delimiter, so data rows shift down by one).
    std::vector<std::string> renderedLines;
    std::string current;
    for (const char ch : rendered) {
        if (ch == '\n') {
            renderedLines.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    renderedLines.push_back(current);

    int renderedRow = (row == 0) ? 0 : row + 1;
    renderedRow = std::min(renderedRow, static_cast<int>(renderedLines.size()) - 1);
    Scintilla::Position lineOffset = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(renderedRow); ++i) {
        lineOffset += static_cast<Scintilla::Position>(renderedLines[i].size()) + 1;
    }
    const auto [cellStart, cellEnd] =
        cellContentSpan(renderedLines[static_cast<std::size_t>(renderedRow)], col);

    const Scintilla::Position selStart = regionStart + lineOffset + cellStart;
    if (moveCaret) {
        call_.SetSelection(regionStart + lineOffset + cellEnd, selStart);
    } else {
        call_.GotoPos(selStart);
    }
    call_.EndUndoAction();
    call_.ScrollCaret();
    return true;
}

bool Editor::navigateTableCell(bool forward)
{
    return reflowTable(/*moveCaret=*/true, forward);
}

void Editor::formatTable()
{
    reflowTable(/*moveCaret=*/false, /*forward=*/true);
}

namespace {

/// Byte offset of the task-mark character (the one between `[` and `]`) on a
/// list line, or npos when the line is not `<indent><marker> [ ] …`.
std::size_t taskMarkOffset(const std::string& body)
{
    std::size_t i = 0;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) {
        ++i;
    }
    if (i >= body.size()) {
        return std::string::npos;
    }
    if (body[i] == '-' || body[i] == '*' || body[i] == '+') {
        ++i;
    } else {
        std::size_t digits = i;
        while (digits < body.size() &&
               std::isdigit(static_cast<unsigned char>(body[digits])) != 0) {
            ++digits;
        }
        if (digits == i || digits >= body.size() || (body[digits] != '.' && body[digits] != ')')) {
            return std::string::npos;
        }
        i = digits + 1;
    }
    if (i >= body.size() || (body[i] != ' ' && body[i] != '\t')) {
        return std::string::npos;
    }
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) {
        ++i;
    }
    if (i + 2 >= body.size() || body[i] != '[' || body[i + 2] != ']') {
        return std::string::npos;
    }
    const char mark = body[i + 1];
    if (mark != ' ' && mark != 'x' && mark != 'X') {
        return std::string::npos;
    }
    return i + 1;
}

} // namespace

void Editor::setTaskChecked(int line, bool checked)
{
    if (line < 0 || line >= lineCount()) {
        return;
    }
    const Scintilla::Position lineStart = call_.PositionFromLine(line);
    const std::string body = call_.StringOfSpan({lineStart, call_.LineEndPosition(line)});
    const std::size_t mark = taskMarkOffset(body);
    if (mark == std::string::npos) {
        return;
    }
    const bool isChecked = body[mark] != ' ';
    if (isChecked == checked) {
        return;
    }
    const Scintilla::Position at = lineStart + static_cast<Scintilla::Position>(mark);
    call_.BeginUndoAction();
    call_.SetTargetRange(at, at + 1);
    call_.ReplaceTarget(1, checked ? "x" : " ");
    call_.EndUndoAction();
}

void Editor::setImagePasteHandler(ImagePasteHandler handler)
{
    imagePasteHandler_ = std::move(handler);
}

bool Editor::handleSmartPaste()
{
    const QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData* mime = clipboard != nullptr ? clipboard->mimeData() : nullptr;
    if (mime == nullptr) {
        return false;
    }

    if (imagePasteHandler_ && mime->hasImage()) {
        const auto image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            const QString markdown = imagePasteHandler_(image);
            if (!markdown.isEmpty()) {
                const QByteArray utf8 = markdown.toUtf8();
                call_.BeginUndoAction();
                call_.ReplaceSel(utf8.constData());
                call_.EndUndoAction();
                return true;
            }
        }
    }

    if (mime->hasText() && call_.SelectionStart() != call_.SelectionEnd()) {
        const std::string url = mime->text().trimmed().toStdString();
        if (url.find_first_of("\r\n") == std::string::npos && looksLikeUrl(url)) {
            const Scintilla::Position s = call_.SelectionStart();
            const std::string label = call_.StringOfSpan({s, call_.SelectionEnd()});

            std::string repl = "[";
            repl += label;
            repl += "](";
            repl += url;
            repl += ")";
            const auto end = s + static_cast<Scintilla::Position>(repl.size());
            call_.BeginUndoAction();
            call_.SetTargetRange(s, call_.SelectionEnd());
            call_.ReplaceTarget(Scintilla::Position(repl.size()), repl.c_str());
            call_.SetSelection(end, end);
            call_.EndUndoAction();
            return true;
        }
    }

    return false;
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

void Editor::setTabWidth(int width)
{
    tabWidth_ = width;
    call_.SetTabWidth(width);
}

void Editor::setWordWrap(bool wrap)
{
    wordWrap_ = wrap;
    call_.SetWrapMode(wrap ? Scintilla::Wrap::Word : Scintilla::Wrap::None);
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
    updateFrontMatterFold();
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

    call_.StyleSetBack(STYLE_BRACELIGHT, sciColour(palette.braceMatch));
    call_.StyleSetBold(STYLE_BRACELIGHT, true);
    call_.StyleSetFore(STYLE_BRACEBAD, sciColour(palette.braceBad));
    call_.StyleSetBold(STYLE_BRACEBAD, true);

    call_.SetEOLMode(Scintilla::EndOfLine::Lf);
    call_.SetTabWidth(tabWidth_);
    call_.SetUseTabs(false);
    call_.SetViewWS(Scintilla::WhiteSpace::Invisible);
    call_.SetWrapMode(wordWrap_ ? Scintilla::Wrap::Word : Scintilla::Wrap::None);
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

    // Fold margin: used only for the YAML front-matter block, so it stays
    // hidden (width 0) until updateFrontMatterFold() finds one.
    call_.SetMarginTypeN(kFoldMargin, Scintilla::MarginType::Symbol);
    call_.SetMarginMaskN(kFoldMargin, Scintilla::MaskFolders);
    call_.SetMarginSensitiveN(kFoldMargin, true);
    call_.SetMarginWidthN(kFoldMargin, frontMatterLastLine_ >= 0 ? kFoldMarginWidth : 0);
    const auto defineFold = [&](Scintilla::MarkerOutline marker, Scintilla::MarkerSymbol symbol) {
        const int n = static_cast<int>(marker);
        call_.MarkerDefine(n, symbol);
        call_.MarkerSetFore(n, sciColour(palette.lineNumberText));
        call_.MarkerSetBack(n, sciColour(palette.background));
    };
    defineFold(Scintilla::MarkerOutline::Folder, Scintilla::MarkerSymbol::BoxPlus);
    defineFold(Scintilla::MarkerOutline::FolderOpen, Scintilla::MarkerSymbol::BoxMinus);
    defineFold(Scintilla::MarkerOutline::FolderEnd, Scintilla::MarkerSymbol::BoxPlusConnected);
    defineFold(Scintilla::MarkerOutline::FolderOpenMid, Scintilla::MarkerSymbol::BoxMinusConnected);
    defineFold(Scintilla::MarkerOutline::FolderMidTail, Scintilla::MarkerSymbol::TCorner);
    defineFold(Scintilla::MarkerOutline::FolderSub, Scintilla::MarkerSymbol::VLine);
    defineFold(Scintilla::MarkerOutline::FolderTail, Scintilla::MarkerSymbol::LCorner);
    call_.SetAutomaticFold(
        static_cast<Scintilla::AutomaticFold>(static_cast<int>(Scintilla::AutomaticFold::Show) |
                                              static_cast<int>(Scintilla::AutomaticFold::Click) |
                                              static_cast<int>(Scintilla::AutomaticFold::Change)));
    call_.SetFoldFlags(Scintilla::FoldFlag::LineAfterContracted);
    call_.SetDefaultFoldDisplayText(" \342\200\246"); // " ..."
    call_.FoldDisplayTextSetStyle(Scintilla::FoldDisplayTextStyle::Standard);

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

void Editor::updateFrontMatterFold()
{
    const frontmatter::FrontMatter front = frontmatter::parse(text());
    frontMatterLastLine_ = front.present ? front.lastLine : -1;

    if (!front.present) {
        call_.SetMarginWidthN(kFoldMargin, 0);
        call_.SetFoldLevel(0, foldLevel(0, /*header=*/false));
        return;
    }

    const int through = std::min(front.lastLine + 1, lineCount() - 1);
    for (int line = 0; line <= through; ++line) {
        const bool inside = line >= 1 && line <= front.lastLine;
        call_.SetFoldLevel(line, foldLevel(inside ? 1 : 0, /*header=*/line == 0));
    }
    call_.SetMarginWidthN(kFoldMargin, kFoldMarginWidth);
}

bool Editor::hasFrontMatter() const
{
    return frontMatterLastLine_ >= 0;
}

bool Editor::isFrontMatterFolded() const
{
    return frontMatterLastLine_ >= 0 && !call_.FoldExpanded(0);
}

void Editor::setFrontMatterFolded(bool folded)
{
    if (frontMatterLastLine_ < 0) {
        return;
    }
    const int caret = cursorLine();
    if (caret >= 0 && caret <= frontMatterLastLine_) {
        return; // don't collapse the block out from under the caret
    }
    call_.FoldLine(0, folded ? Scintilla::FoldAction::Contract : Scintilla::FoldAction::Expand);
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
            updateFrontMatterFold();
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
            updateBraceHighlight();
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

int Editor::matchingBrace(int position) const
{
    return static_cast<int>(call_.BraceMatch(position, 0));
}

namespace {

bool isBracket(int ch)
{
    return ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
}

/// Split a line's text into its leading indentation, a Markdown list marker
/// ("- ", "* ", "+ " or "N. ") if present, and the remaining content.
struct LinePrefix
{
    std::string indent;
    std::string marker;
    std::string rest;
};

LinePrefix analyseLine(const std::string& line)
{
    LinePrefix prefix;
    const std::size_t contentStart = line.find_first_not_of(" \t");
    if (contentStart == std::string::npos) {
        prefix.indent = line;
        return prefix;
    }
    prefix.indent = line.substr(0, contentStart);
    const std::string body = line.substr(contentStart);

    if (body.size() >= 2 && body[1] == ' ' &&
        (body[0] == '-' || body[0] == '*' || body[0] == '+')) {
        prefix.marker = body.substr(0, 2);
        prefix.rest = body.substr(2);
        return prefix;
    }
    std::size_t digits = 0;
    while (digits < body.size() && std::isdigit(static_cast<unsigned char>(body[digits])) != 0) {
        ++digits;
    }
    if (digits > 0 && digits + 1 < body.size() && body[digits + 1] == ' ' &&
        (body[digits] == '.' || body[digits] == ')')) {
        prefix.marker = body.substr(0, digits + 2);
        prefix.rest = body.substr(digits + 2);
        return prefix;
    }
    prefix.rest = body;
    return prefix;
}

std::string bumpedMarker(const std::string& marker)
{
    if (std::isdigit(static_cast<unsigned char>(marker[0])) == 0) {
        return marker;
    }
    std::size_t digits = 0;
    while (std::isdigit(static_cast<unsigned char>(marker[digits])) != 0) {
        ++digits;
    }
    std::string bumped = std::to_string(std::stol(marker.substr(0, digits)) + 1);
    bumped += marker.substr(digits);
    return bumped;
}

} // namespace

void Editor::updateBraceHighlight()
{
    const Scintilla::Position caret = call_.CurrentPos();
    Scintilla::Position bracePos = -1;
    for (const Scintilla::Position candidate : {caret - 1, caret}) {
        if (candidate >= 0 && isBracket(call_.CharAt(candidate))) {
            bracePos = candidate;
            break;
        }
    }

    if (bracePos < 0) {
        call_.BraceHighlight(-1, -1);
        return;
    }
    const Scintilla::Position match = call_.BraceMatch(bracePos, 0);
    if (match < 0) {
        call_.BraceBadLight(bracePos);
    } else {
        call_.BraceHighlight(bracePos, match);
    }
}

bool Editor::insertSmartNewline()
{
    if (call_.Selections() != 1 || call_.SelectionStart() != call_.SelectionEnd()) {
        return false;
    }

    const Scintilla::Position caret = call_.CurrentPos();
    const Scintilla::Line line = call_.LineFromPosition(caret);
    const Scintilla::Position lineStart = call_.PositionFromLine(line);
    const std::string text = call_.StringOfSpan({lineStart, call_.LineEndPosition(line)});
    const LinePrefix prefix = analyseLine(text);

    const bool caretPastPrefix =
        caret - lineStart >= Scintilla::Position(prefix.indent.size() + prefix.marker.size());

    if (!prefix.marker.empty() && prefix.rest.find_first_not_of(" \t") == std::string::npos &&
        caretPastPrefix) {
        // Enter on an otherwise-empty list item: drop the marker, stay put.
        call_.SetTargetRange(lineStart, call_.LineEndPosition(line));
        call_.ReplaceTarget(Scintilla::Position(prefix.indent.size()), prefix.indent.c_str());
        call_.GotoPos(lineStart + Scintilla::Position(prefix.indent.size()));
        return true;
    }

    std::string insert = "\n";
    insert += prefix.indent;
    if (!prefix.marker.empty() && caretPastPrefix) {
        insert += bumpedMarker(prefix.marker);
    }
    call_.BeginUndoAction();
    call_.AddText(Scintilla::Position(insert.size()), insert.c_str());
    call_.EndUndoAction();
    return true;
}

void Editor::keyPressEvent(QKeyEvent* event)
{
    const bool plainReturn = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
                             event->modifiers() == Qt::NoModifier;
    if (plainReturn && insertSmartNewline()) {
        event->accept();
        return;
    }

    const bool paste =
        (event->matches(QKeySequence::Paste) ||
         (event->key() == Qt::Key_Insert && event->modifiers() == Qt::ShiftModifier));
    if (paste && handleSmartPaste()) {
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier &&
        navigateTableCell(/*forward=*/true)) {
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Backtab &&
        (event->modifiers() & ~Qt::ShiftModifier) == Qt::NoModifier &&
        navigateTableCell(/*forward=*/false)) {
        event->accept();
        return;
    }

    ScintillaEditBase::keyPressEvent(event);
}

} // namespace hungryeditor
