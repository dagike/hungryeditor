#include "editor/DocumentManager.h"

#include <memory>
#include <utility>

#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

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

    watcher_ = new QFileSystemWatcher(this);
    pollTimer_ = new QTimer(this);
    pollTimer_->setSingleShot(true);
    pollTimer_->setInterval(60);
    connect(pollTimer_, &QTimer::timeout, this, &DocumentManager::pollExternalChanges);
    const auto schedulePoll = [this] { pollTimer_->start(); };
    connect(watcher_, &QFileSystemWatcher::fileChanged, this, schedulePoll);
    connect(watcher_, &QFileSystemWatcher::directoryChanged, this, schedulePoll);

    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setInterval(2000);
    connect(autosaveTimer_, &QTimer::timeout, this, &DocumentManager::autosaveDirtyDocuments);

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
    captureDiskState(raw);
    refreshWatch();

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
    dropDraft(document);
    // Record the state we just wrote so the watcher notification our own save
    // triggers is recognised as ours, then re-arm the watch (the atomic
    // rename QSaveFile does drops the file from the watcher).
    captureDiskState(document);
    refreshWatch();
    if (error != nullptr) {
        *error = {};
    }
    emit modifiedChanged(index, false);
    return true;
}

bool DocumentManager::reloadDocument(Document* document, FileError* error)
{
    const int index = indexOf(document);
    if (index < 0 || document->isUntitled()) {
        if (error != nullptr) {
            *error = {false, tr("The document has no file to reload from.")};
        }
        return false;
    }

    FileError localError;
    const TextDocument loaded = loadFile(document->path(), &localError);
    if (!localError.ok) {
        if (error != nullptr) {
            *error = localError;
        }
        return false;
    }

    if (index != currentIndex_) {
        setCurrentIndex(index);
    }
    const int caretLine = editor_->cursorLine();

    document->setEncoding(loaded.encoding);
    document->setLineEnding(loaded.lineEnding);
    editor_->setText(loaded.text);
    editor_->call().EmptyUndoBuffer();
    editor_->markClean();
    if (editor_->lineCount() > 0) {
        editor_->setCursorPosition(qMin(caretLine, editor_->lineCount() - 1), 0);
    }
    document->setModified(false);
    dropDraft(document);
    captureDiskState(document);
    refreshWatch();

    if (error != nullptr) {
        *error = {};
    }
    emit modifiedChanged(index, false);
    return true;
}

void DocumentManager::pollExternalChanges()
{
    for (int i = 0; i < count(); ++i) {
        Document* document = documents_[static_cast<std::size_t>(i)].get();
        if (document->isUntitled()) {
            continue;
        }
        const QFileInfo info(document->path());

        if (!document->hasDiskState()) {
            // The file went missing earlier; report it if it has come back.
            if (info.exists()) {
                document->recordDiskState(info.size(), info.lastModified());
                emit fileChangedExternally(i);
            }
            continue;
        }

        if (!info.exists()) {
            document->clearDiskState();
            emit fileRemovedExternally(i);
            continue;
        }

        if (!document->matchesDiskState(info.size(), info.lastModified())) {
            document->recordDiskState(info.size(), info.lastModified());
            emit fileChangedExternally(i);
        }
    }
    refreshWatch();
}

void DocumentManager::captureDiskState(Document* document)
{
    const QFileInfo info(document->path());
    if (info.exists()) {
        document->recordDiskState(info.size(), info.lastModified());
    } else {
        document->clearDiskState();
    }
}

void DocumentManager::refreshWatch()
{
    const QStringList tracked = watcher_->files() + watcher_->directories();
    if (!tracked.isEmpty()) {
        watcher_->removePaths(tracked);
    }

    QStringList wanted;
    for (const auto& document : documents_) {
        if (document->isUntitled()) {
            continue;
        }
        const QFileInfo info(document->path());
        const QString file = info.absoluteFilePath();
        if (info.exists() && !wanted.contains(file)) {
            wanted.append(file);
        }
        const QString parent = info.absolutePath();
        if (!parent.isEmpty() && QFileInfo::exists(parent) && !wanted.contains(parent)) {
            wanted.append(parent);
        }
    }
    if (!wanted.isEmpty()) {
        watcher_->addPaths(wanted);
    }
}

void DocumentManager::setDraftDirectory(const QString& directory)
{
    draftStore_ = std::make_unique<DraftStore>(directory);
    if (!autosaveTimer_->isActive()) {
        autosaveTimer_->start();
    }
}

void DocumentManager::setAutosaveInterval(int milliseconds)
{
    autosaveTimer_->setInterval(milliseconds);
}

void DocumentManager::autosaveDirtyDocuments()
{
    if (!draftStore_) {
        return;
    }
    for (int i = 0; i < count(); ++i) {
        Document* document = documents_[static_cast<std::size_t>(i)].get();
        const QString id = document->draftId();

        if (!document->isModified()) {
            if (draftHashes_.remove(id) > 0) {
                draftStore_->remove(id);
            }
            continue;
        }

        const QString text = (i == currentIndex_) ? editor_->text() : document->snapshotText();
        const std::size_t hash = qHash(text);
        const auto existing = draftHashes_.constFind(id);
        if (existing != draftHashes_.constEnd() && existing.value() == hash) {
            continue;
        }

        Draft draft;
        draft.id = id;
        draft.originalPath = document->path();
        draft.text = text;
        draft.encoding = document->encoding();
        draft.lineEnding = document->lineEnding();
        if (draftStore_->write(draft)) {
            draftHashes_.insert(id, hash);
        }
    }
}

QList<Draft> DocumentManager::pendingDrafts() const
{
    return draftStore_ ? draftStore_->loadAll() : QList<Draft>();
}

void DocumentManager::restoreDrafts(const QList<Draft>& drafts)
{
    for (const Draft& draft : drafts) {
        const bool untitled = draft.originalPath.isEmpty();
        auto document =
            std::make_unique<Document>(createScintillaDocument(editor_), draft.originalPath,
                                       untitled ? nextUntitledNumber_++ : 0);
        document->setEncoding(draft.encoding);
        document->setLineEnding(draft.lineEnding);
        document->adoptDraftId(draft.id);
        Document* raw = addDocument(std::move(document));

        setCurrentIndex(indexOf(raw));
        editor_->setText(draft.text);
        // Deliberately no EmptyUndoBuffer()/markClean(): a draft is unsaved
        // work. Leaving the insert on the undo stack keeps the buffer off its
        // save point, so it stays marked modified.
        if (!raw->isUntitled()) {
            captureDiskState(raw);
        }
        draftHashes_.insert(draft.id, qHash(draft.text));
    }
    refreshWatch();
}

void DocumentManager::clearDrafts()
{
    if (draftStore_) {
        draftStore_->clear();
    }
    draftHashes_.clear();
}

void DocumentManager::dropDraft(const Document* document)
{
    if (!draftStore_ || document == nullptr) {
        return;
    }
    draftStore_->remove(document->draftId());
    draftHashes_.remove(document->draftId());
}

void DocumentManager::closeDocument(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    dropDraft(documents_[static_cast<std::size_t>(index)].get());
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
    refreshWatch();

    emit documentClosed(index);
    emit currentChanged(currentIndex_);
}

void DocumentManager::moveDocument(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= count() || to >= count()) {
        return;
    }

    std::unique_ptr<Document> moved = std::move(documents_[static_cast<std::size_t>(from)]);
    documents_.erase(documents_.begin() + from);
    documents_.insert(documents_.begin() + to, std::move(moved));

    if (currentIndex_ == from) {
        currentIndex_ = to;
    } else if (from < currentIndex_ && currentIndex_ <= to) {
        --currentIndex_;
    } else if (to <= currentIndex_ && currentIndex_ < from) {
        ++currentIndex_;
    }

    emit documentMoved(from, to);
}

void DocumentManager::setCurrentIndex(int index)
{
    if (index < 0 || index >= count() || index == currentIndex_) {
        return;
    }
    // The buffer we are leaving cannot change while it is off screen, so this
    // snapshot stays exact and lets autosave read it without a pointer swap.
    if (Document* leaving = current()) {
        leaving->setSnapshotText(editor_->text());
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
