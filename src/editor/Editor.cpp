#include "editor/Editor.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <QColor>
#include <QFontDatabase>

#include <Scintilla.h> // STYLE_DEFAULT / STYLE_LINENUMBER
#include <ScintillaMessages.h>
#include <ScintillaStructures.h>
#include <ScintillaTypes.h>
#include <tree_sitter/api.h>

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
};

constexpr int kLineNumberMargin = 0;
constexpr int kSymbolMargin = 1;
constexpr int kFoldMargin = 2;
constexpr int kMinLineDigits = 3;
constexpr int kTabWidth = 4;
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
                          static_cast<qsizetype>(queries::kMarkdownHighlights.size())));
    connect(highlight_, &HighlightController::highlighted, this, &Editor::applyHighlight);
    connect(this, &Editor::textChanged, this, [this] { highlight_->submit(text()); });
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

void Editor::setEditorFont(const QFont& font)
{
    font_ = font;
    applyVisualDefaults();
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

    call_.SetTabWidth(kTabWidth);
    call_.SetUseTabs(false);
    call_.SetViewWS(Scintilla::WhiteSpace::Invisible);
    call_.SetWrapMode(Scintilla::Wrap::None);
    call_.SetScrollWidthTracking(true);
    call_.SetScrollWidth(1);

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

void Editor::applyHighlight(const HighlightResult& result)
{
    if (!result.ok) {
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
