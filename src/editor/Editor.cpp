#include "editor/Editor.h"

#include <string>
#include <string_view>

#include <ScintillaMessages.h>
#include <ScintillaStructures.h>
#include <ScintillaTypes.h>

namespace hungryeditor {

namespace {
using Scintilla::Message;

Scintilla::sptr_t sciSend(const ScintillaEditBase& editor, Message msg)
{
    // send() is const on ScintillaEditBase.
    return editor.send(static_cast<unsigned int>(msg));
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

    connect(this, &ScintillaEditBase::notify, this, &Editor::onNotify);
}

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

    default:
        break;
    }
}

} // namespace hungryeditor
