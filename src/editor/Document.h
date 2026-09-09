#pragma once

#include <QDateTime>
#include <QString>

#include "io/TextFile.h" // Encoding, LineEnding

namespace Scintilla {
class IDocumentEditable;
}

namespace hungryeditor {

/// One open buffer: a Scintilla document pointer plus the metadata needed to
/// save it back where it came from.
///
/// Documents are owned by DocumentManager, which creates and releases the
/// underlying Scintilla document. The Editor widget shows one at a time via
/// Editor::attachDocument(); switching is an O(1) pointer swap that keeps each
/// buffer's undo history and caret intact.
class Document
{
public:
    Document(Scintilla::IDocumentEditable* pointer, QString path, int untitledNumber);

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    Scintilla::IDocumentEditable* pointer() const { return pointer_; }

    QString path() const { return path_; }
    void setPath(const QString& path) { path_ = path; }
    bool isUntitled() const { return path_.isEmpty(); }

    /// File name for the title and (later) the tab: the base name, or
    /// "Untitled" / "Untitled N" for a document that has never been saved.
    QString displayName() const;

    Encoding encoding() const { return encoding_; }
    void setEncoding(Encoding encoding) { encoding_ = encoding; }
    LineEnding lineEnding() const { return lineEnding_; }
    void setLineEnding(LineEnding lineEnding) { lineEnding_ = lineEnding; }

    /// Cached unsaved-changes flag, kept in step with Scintilla's save point
    /// by DocumentManager.
    bool isModified() const { return modified_; }
    void setModified(bool modified) { modified_ = modified; }

    /// The file's size and modification time as last seen on disk — recorded
    /// on load, save and reload so DocumentManager can tell an external edit
    /// from our own write. Cleared while the file does not exist.
    void recordDiskState(qint64 size, const QDateTime& modified);
    void clearDiskState();
    bool hasDiskState() const { return diskSize_ >= 0; }
    bool matchesDiskState(qint64 size, const QDateTime& modified) const;

private:
    Scintilla::IDocumentEditable* pointer_ = nullptr;
    QString path_;
    int untitledNumber_ = 0;
    Encoding encoding_ = Encoding::Utf8;
    LineEnding lineEnding_ = LineEnding::Lf;
    bool modified_ = false;
    qint64 diskSize_ = -1;
    QDateTime diskModified_;
};

} // namespace hungryeditor
