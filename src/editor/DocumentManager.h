#pragma once

#include <memory>
#include <vector>

#include <QObject>

#include "io/TextFile.h" // FileError

class QFileSystemWatcher;
class QTimer;

namespace hungryeditor {

class Document;
class Editor;

/// Owns every open Document and the notion of which one is current, and keeps
/// the Editor widget attached to it. All open/save/close/switch operations go
/// through here so the rest of the application never touches Scintilla
/// document pointers directly.
class DocumentManager : public QObject
{
    Q_OBJECT

public:
    explicit DocumentManager(Editor* editor, QObject* parent = nullptr);
    ~DocumentManager() override;

    int count() const { return static_cast<int>(documents_.size()); }
    Document* documentAt(int index) const;
    Document* current() const;
    int currentIndex() const { return currentIndex_; }
    int indexOf(const Document* document) const;

    /// Create an empty untitled document and make it current.
    Document* newDocument();

    /// Open `path`. If it is already open, just activate that document.
    /// Returns nullptr and fills `error` on an I/O failure.
    Document* openDocument(const QString& path, FileError* error = nullptr);

    /// Write `document` to `path`, updating its metadata and clean state.
    bool saveDocument(Document* document, const QString& path, FileError* error = nullptr);

    /// Re-read `document` from its file, replacing the buffer, emptying undo
    /// and marking it clean. The caret line is kept where it can be. Returns
    /// false (and fills `error`) for an untitled document or an I/O failure.
    bool reloadDocument(Document* document, FileError* error = nullptr);

    /// Re-stat every open file and emit fileChangedExternally() /
    /// fileRemovedExternally() for the ones that no longer match what we last
    /// saw. Driven by a QFileSystemWatcher; called directly from tests.
    void pollExternalChanges();

    /// Close the document at `index`. Always leaves at least one document
    /// open: closing the last one replaces it with a fresh untitled buffer.
    void closeDocument(int index);

    /// Reorder the document list, moving the entry at `from` to `to`. The
    /// current document stays current (its index is adjusted). Scintilla
    /// documents are untouched.
    void moveDocument(int from, int to);

    void setCurrentIndex(int index);

signals:
    void documentAdded(int index);
    void documentClosed(int index);
    void documentMoved(int from, int to);
    void currentChanged(int index);
    void modifiedChanged(int index, bool modified);
    void fileChangedExternally(int index);
    void fileRemovedExternally(int index);

private:
    Document* addDocument(std::unique_ptr<Document> document);
    void releasePointer(Document* document);
    void onEditorModifiedChanged(bool modified);
    void captureDiskState(Document* document);
    void refreshWatch();

    Editor* editor_ = nullptr;
    std::vector<std::unique_ptr<Document>> documents_;
    int currentIndex_ = -1;
    int nextUntitledNumber_ = 0;
    QFileSystemWatcher* watcher_ = nullptr;
    QTimer* pollTimer_ = nullptr;
};

} // namespace hungryeditor
