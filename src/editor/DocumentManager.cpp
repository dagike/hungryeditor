#include "editor/DocumentManager.h"

#include <memory>
#include <utility>

#include <QFileInfo>

#include "editor/Document.h"
#include "editor/Editor.h"
#include "io/TextFile.h"

namespace hungryeditor {

namespace {

/// A comparable key for a path: its canonical form when the file exists,
/// otherwise its absolute form.
QString pathKey(const QString& path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QFileInfo(path).absoluteFilePath() : canonical;
}

Scintilla::IDocumentEditable* createScintillaDocument(Editor* editor)
{
    return editor->call().CreateDocument(0, Scintilla::DocumentOption::Default);
}

} // namespace

DocumentManager::DocumentManager(Editor* editor, QObject* parent) : QObject(parent), editor_(editor)
{
    connect(editor_, &Editor::modifiedChanged, this, &DocumentManager::onEditorModifiedChanged);
    newDocument();
}

DocumentManager::~DocumentManager()
{
    // MainWindow owns both the editor and this manager and destroys the manager
    // first, so the editor is still alive here. Releasing every owned reference
    // leaves only the view's reference on the current document, which the
    // editor drops when it is destroyed.
    for (const auto& document : documents_) {
        releasePointer(document.get());
    }
}

Document* DocumentManager::documentAt(int index) const
{
    if (index < 0 || index >= count()) {
        return nullptr;
    }
    return documents_[static_cast<std::size_t>(index)].get();
}

Document* DocumentManager::current() const
{
    return documentAt(currentIndex_);
}

int DocumentManager::indexOf(const Document* document) const
{
    for (int i = 0; i < count(); ++i) {
        if (documents_[static_cast<std::size_t>(i)].get() == document) {
            return i;
        }
    }
    return -1;
}

Document* DocumentManager::addDocument(std::unique_ptr<Document> document)
{
    Document* raw = document.get();
    documents_.push_back(std::move(document));
    emit documentAdded(count() - 1);
    return raw;
}

void DocumentManager::releasePointer(Document* document)
{
    if (document != nullptr && document->pointer() != nullptr) {
        editor_->call().ReleaseDocument(document->pointer());
    }
}

Document* DocumentManager::newDocument()
{
    auto document = std::make_unique<Document>(createScintillaDocument(editor_), QString(),
                                               nextUntitledNumber_++);
    Document* raw = addDocument(std::move(document));
    setCurrentIndex(indexOf(raw));
    return raw;
}

Document* DocumentManager::openDocument(const QString& path, FileError* error)
{
    const QString key = pathKey(path);
    for (int i = 0; i < count(); ++i) {
        Document* candidate = documents_[static_cast<std::size_t>(i)].get();
        if (!candidate->isUntitled() && pathKey(candidate->path()) == key) {
            setCurrentIndex(i);
            if (error != nullptr) {
                *error = {};
            }
            return candidate;
        }
    }

    FileError localError;
    const TextDocument loaded = loadFile(path, &localError);
    if (!localError.ok) {
        if (error != nullptr) {
            *error = localError;
        }
        return nullptr;
    }

    auto document = std::make_unique<Document>(createScintillaDocument(editor_), path, 0);
    document->setEncoding(loaded.encoding);
    document->setLineEnding(loaded.lineEnding);
    Document* raw = addDocument(std::move(document));

    setCurrentIndex(indexOf(raw));
    editor_->setText(loaded.text);
    editor_->call().EmptyUndoBuffer();
    editor_->markClean();
    raw->setModified(false);

    if (error != nullptr) {
        *error = {};
    }
    return raw;
}

bool DocumentManager::saveDocument(Document* document, const QString& path, FileError* error)
{
    const int index = indexOf(document);
    if (index < 0) {
        if (error != nullptr) {
            *error = {false, tr("The document is not open.")};
        }
        return false;
    }
    if (index != currentIndex_) {
        setCurrentIndex(index);
    }

    const TextDocument out{editor_->text(), document->encoding(), document->lineEnding()};
    FileError localError;
    if (!saveFile(path, out, &localError)) {
        if (error != nullptr) {
            *error = localError;
        }
        return false;
    }

    document->setPath(path);
    editor_->markClean();
    document->setModified(false);
    if (error != nullptr) {
        *error = {};
    }
    emit modifiedChanged(index, false);
    return true;
}

void DocumentManager::closeDocument(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    const bool closingCurrent = (index == currentIndex_);

    if (count() == 1) {
        // Replace the sole document with a fresh untitled buffer.
        addDocument(std::make_unique<Document>(createScintillaDocument(editor_), QString(),
                                               nextUntitledNumber_++));
        currentIndex_ = -1;
        setCurrentIndex(1);
    } else if (closingCurrent) {
        const int neighbour = (index + 1 < count()) ? index + 1 : index - 1;
        currentIndex_ = -1;
        setCurrentIndex(neighbour);
    }

    releasePointer(documents_[static_cast<std::size_t>(index)].get());
    documents_.erase(documents_.begin() + index);
    if (currentIndex_ > index) {
        --currentIndex_;
    }

    emit documentClosed(index);
    emit currentChanged(currentIndex_);
}

void DocumentManager::setCurrentIndex(int index)
{
    if (index < 0 || index >= count() || index == currentIndex_) {
        return;
    }
    currentIndex_ = index;
    Document* document = documents_[static_cast<std::size_t>(index)].get();
    editor_->attachDocument(document);
    document->setModified(editor_->isModified());
    emit currentChanged(index);
}

void DocumentManager::onEditorModifiedChanged(bool modified)
{
    if (currentIndex_ < 0 || currentIndex_ >= count()) {
        return;
    }
    documents_[static_cast<std::size_t>(currentIndex_)]->setModified(modified);
    emit modifiedChanged(currentIndex_, modified);
}

} // namespace hungryeditor
