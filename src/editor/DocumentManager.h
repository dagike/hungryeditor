#pragma once

#include <memory>
#include <vector>

#include <QObject>

#include "io/TextFile.h" // FileError

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

    /// Close the document at `index`. Always leaves at least one document
    /// open: closing the last one replaces it with a fresh untitled buffer.
    void closeDocument(int index);

    void setCurrentIndex(int index);

signals:
    void documentAdded(int index);
    void documentClosed(int index);
    void currentChanged(int index);
    void modifiedChanged(int index, bool modified);

private:
    Document* addDocument(std::unique_ptr<Document> document);
    void releasePointer(Document* document);
    void onEditorModifiedChanged(bool modified);

    Editor* editor_ = nullptr;
    std::vector<std::unique_ptr<Document>> documents_;
    int currentIndex_ = -1;
    int nextUntitledNumber_ = 0;
};

} // namespace hungryeditor
