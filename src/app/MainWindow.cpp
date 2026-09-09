#include "app/MainWindow.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "app/TabBar.h"
#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"

#ifndef HUNGRYEDITOR_VERSION
#define HUNGRYEDITOR_VERSION "0.0.0"
#endif

namespace hungryeditor {

namespace {
const QString kFileFilter =
    QStringLiteral("Markdown (*.md *.markdown *.mkd);;Text files (*.txt);;All files (*)");

/// Local filesystem paths carried by a drag's mime data, in drop order.
QStringList localFilesFromMime(const QMimeData* mime)
{
    QStringList paths;
    if (mime == nullptr || !mime->hasUrls()) {
        return paths;
    }
    for (const QUrl& url : mime->urls()) {
        const QString local = url.toLocalFile();
        if (!local.isEmpty()) {
            paths.append(local);
        }
    }
    return paths;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    resize(1000, 720);
    setAcceptDrops(true);

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tabBar_ = new TabBar(container);
    editor_ = new Editor(container);
    layout->addWidget(tabBar_);
    layout->addWidget(editor_);
    setCentralWidget(container);

    documents_ = std::make_unique<DocumentManager>(editor_);

    buildMenus();

    connect(documents_.get(), &DocumentManager::documentAdded, this, &MainWindow::onDocumentAdded);
    connect(documents_.get(), &DocumentManager::documentClosed, this,
            &MainWindow::onDocumentClosed);
    connect(documents_.get(), &DocumentManager::currentChanged, this,
            &MainWindow::onCurrentChanged);
    connect(documents_.get(), &DocumentManager::modifiedChanged, this,
            [this](int index, bool modified) {
                if (index == documents_->currentIndex()) {
                    saveAction_->setEnabled(modified);
                }
                syncTabText(index);
                updateWindowTitle();
            });

    connect(tabBar_, &QTabBar::currentChanged, this, [this](int index) {
        if (!syncingTabs_ && !reorderingTabs_ && index >= 0) {
            documents_->setCurrentIndex(index);
        }
    });
    connect(documents_.get(), &DocumentManager::fileChangedExternally, this,
            &MainWindow::onFileChangedExternally);
    connect(documents_.get(), &DocumentManager::fileRemovedExternally, this,
            &MainWindow::onFileRemovedExternally);

    connect(tabBar_, &QTabBar::tabCloseRequested, this, &MainWindow::closeDocumentAt);
    connect(tabBar_, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (syncingTabs_) {
            return;
        }
        reorderingTabs_ = true;
        documents_->moveDocument(from, to);
        documents_->setCurrentIndex(tabBar_->currentIndex());
        reorderingTabs_ = false;
    });

    // A file dropped onto the text area comes through Scintilla as a URI.
    connect(editor_, &ScintillaEditBase::uriDropped, this, [this](const QString& uri) {
        const QString local = QUrl(uri).toLocalFile();
        if (!local.isEmpty() && !openFiles({local})) {
            QMessageBox::warning(this, tr("Open Failed"), lastError_);
        }
    });

    primeTabs();
    updateWindowTitle();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* newAction = fileMenu->addAction(tr("&New"), this, &MainWindow::newDocument);
    newAction->setShortcut(QKeySequence::New);
    newAction->setObjectName(QStringLiteral("action.new"));

    QAction* openAction = fileMenu->addAction(tr("&Open…"), this, &MainWindow::openFileDialog);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setObjectName(QStringLiteral("action.open"));

    saveAction_ = fileMenu->addAction(tr("&Save"), this, &MainWindow::save);
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setObjectName(QStringLiteral("action.save"));
    saveAction_->setEnabled(false);

    QAction* saveAsAction = fileMenu->addAction(tr("Save &As…"), this, &MainWindow::saveAsDialog);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    saveAsAction->setObjectName(QStringLiteral("action.saveAs"));

    QAction* closeAction =
        fileMenu->addAction(tr("&Close"), this, &MainWindow::closeCurrentDocument);
    closeAction->setShortcut(QKeySequence::Close);
    closeAction->setObjectName(QStringLiteral("action.close"));

    fileMenu->addSeparator();

    QAction* nextAction =
        fileMenu->addAction(tr("&Next Document"), this, &MainWindow::nextDocument);
    nextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    nextAction->setObjectName(QStringLiteral("action.nextDocument"));

    QAction* prevAction =
        fileMenu->addAction(tr("&Previous Document"), this, &MainWindow::previousDocument);
    prevAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    prevAction->setObjectName(QStringLiteral("action.previousDocument"));

    fileMenu->addSeparator();

    QAction* quitAction = fileMenu->addAction(tr("&Quit"), qApp, &QApplication::quit);
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);
    quitAction->setObjectName(QStringLiteral("action.quit"));

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction =
        helpMenu->addAction(tr("&About hungryeditor"), this, &MainWindow::showAbout);
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setObjectName(QStringLiteral("action.about"));
}

void MainWindow::primeTabs()
{
    syncingTabs_ = true;
    for (int i = 0; i < documents_->count(); ++i) {
        tabBar_->addTab(documents_->documentAt(i)->displayName());
        syncTabText(i);
    }
    tabBar_->setCurrentIndex(documents_->currentIndex());
    syncingTabs_ = false;
    tabBar_->setVisible(documents_->count() > 1);
}

void MainWindow::syncTabText(int index)
{
    const Document* document = documents_->documentAt(index);
    if (document == nullptr || index >= tabBar_->count()) {
        return;
    }
    const QString name = document->displayName();
    tabBar_->setTabText(index, document->isModified() ? QStringLiteral("*") + name : name);
    tabBar_->setTabToolTip(index, document->isUntitled() ? name : document->path());
}

void MainWindow::onDocumentAdded(int index)
{
    syncingTabs_ = true;
    tabBar_->insertTab(index, documents_->documentAt(index)->displayName());
    syncTabText(index);
    syncingTabs_ = false;
    tabBar_->setVisible(documents_->count() > 1);
}

void MainWindow::onDocumentClosed(int index)
{
    syncingTabs_ = true;
    tabBar_->removeTab(index);
    syncingTabs_ = false;
    tabBar_->setVisible(documents_->count() > 1);
}

void MainWindow::onCurrentChanged(int index)
{
    syncingTabs_ = true;
    if (index >= 0 && index < tabBar_->count()) {
        tabBar_->setCurrentIndex(index);
    }
    syncingTabs_ = false;
    updateWindowTitle();
}

QString MainWindow::currentPath() const
{
    const Document* document = documents_->current();
    return document != nullptr ? document->path() : QString();
}

bool MainWindow::openPath(const QString& path)
{
    return openFiles({path});
}

bool MainWindow::openFiles(const QStringList& paths)
{
    QStringList failures;
    Document* firstOpened = nullptr;
    for (const QString& path : paths) {
        FileError error;
        Document* document = documents_->openDocument(path, &error);
        if (document == nullptr) {
            failures.append(QStringLiteral("%1: %2").arg(path, error.message));
        } else if (firstOpened == nullptr) {
            firstOpened = document;
        }
    }

    if (firstOpened != nullptr) {
        documents_->setCurrentIndex(documents_->indexOf(firstOpened));

        // Drop the blank buffer the window starts with so a command-line or
        // drag-and-drop open does not leave a stray "Untitled" tab behind.
        Document* first = documents_->documentAt(0);
        if (documents_->count() > 1 && first->isUntitled() && !first->isModified()) {
            documents_->closeDocument(0);
        }
    }

    lastError_ = failures.join(QLatin1Char('\n'));
    return failures.isEmpty();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!localFilesFromMime(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QStringList paths = localFilesFromMime(event->mimeData());
    if (paths.isEmpty()) {
        return;
    }
    event->acceptProposedAction();
    if (!openFiles(paths)) {
        QMessageBox::warning(this, tr("Open Failed"), lastError_);
    }
}

bool MainWindow::savePath(const QString& path)
{
    Document* document = documents_->current();
    if (document == nullptr) {
        return false;
    }
    FileError error;
    if (!documents_->saveDocument(document, path, &error)) {
        lastError_ = error.message;
        return false;
    }
    lastError_.clear();
    return true;
}

void MainWindow::closeDocumentAt(int index)
{
    const Document* document = documents_->documentAt(index);
    if (document == nullptr) {
        return;
    }
    if (document->isModified()) {
        const QMessageBox::StandardButton choice = QMessageBox::question(
            this, tr("Discard Changes?"),
            tr("“%1” has unsaved changes. Close it anyway?").arg(document->displayName()),
            QMessageBox::Discard | QMessageBox::Cancel);
        if (choice != QMessageBox::Discard) {
            return;
        }
    }
    documents_->closeDocument(index);
}

bool MainWindow::reloadDocumentAt(int index)
{
    Document* document = documents_->documentAt(index);
    if (document == nullptr) {
        return false;
    }
    FileError error;
    if (!documents_->reloadDocument(document, &error)) {
        lastError_ = error.message;
        return false;
    }
    lastError_.clear();
    updateWindowTitle();
    return true;
}

void MainWindow::onFileChangedExternally(int index)
{
    Document* document = documents_->documentAt(index);
    if (document == nullptr) {
        return;
    }
    reportedMissingFiles_.remove(document->path());

    if (!document->isModified()) {
        reloadDocumentAt(index); // no local work to lose — just take the new text
        return;
    }

    if (pendingReloadPrompts_.contains(document)) {
        return;
    }
    pendingReloadPrompts_.insert(document);

    // Ask on the next event-loop turn rather than interrupting an edit from
    // inside a change notification.
    QTimer::singleShot(0, this, [this, document] {
        pendingReloadPrompts_.remove(document);
        const int current = documents_->indexOf(document);
        if (current < 0) {
            return;
        }
        if (!document->isModified()) {
            reloadDocumentAt(current);
            return;
        }
        if (confirmReloadOverLocalChanges(document->displayName())) {
            if (!reloadDocumentAt(current)) {
                QMessageBox::warning(this, tr("Reload Failed"), lastError_);
            }
        }
    });
}

void MainWindow::onFileRemovedExternally(int index)
{
    Document* document = documents_->documentAt(index);
    if (document == nullptr || document->path().isEmpty()) {
        return;
    }
    if (reportedMissingFiles_.contains(document->path())) {
        return;
    }
    reportedMissingFiles_.insert(document->path());

    QTimer::singleShot(0, this, [this, document] {
        if (documents_->indexOf(document) < 0) {
            return;
        }
        QMessageBox::warning(
            this, tr("File Removed"),
            tr("“%1” no longer exists on disk. It stays open here — save it to write it back.")
                .arg(document->displayName()));
    });
}

bool MainWindow::confirmReloadOverLocalChanges(const QString& name)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("File Changed on Disk"));
    box.setText(tr("“%1” has changed on disk.").arg(name));
    box.setInformativeText(tr("Reload it and lose your unsaved changes?"));
    QPushButton* reload = box.addButton(tr("Reload"), QMessageBox::AcceptRole);
    box.addButton(tr("Keep My Changes"), QMessageBox::RejectRole);
    box.exec();
    return box.clickedButton() == reload;
}

void MainWindow::newDocument()
{
    documents_->newDocument();
}

void MainWindow::openFileDialog()
{
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Open File"), currentPath(), kFileFilter);
    if (path.isEmpty()) {
        return;
    }
    if (!openPath(path)) {
        QMessageBox::warning(this, tr("Open Failed"), lastError_);
    }
}

void MainWindow::save()
{
    const Document* document = documents_->current();
    if (document == nullptr || document->isUntitled()) {
        saveAsDialog();
        return;
    }
    if (!savePath(document->path())) {
        QMessageBox::warning(this, tr("Save Failed"), lastError_);
    }
}

void MainWindow::saveAsDialog()
{
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Save File As"), currentPath(), kFileFilter);
    if (path.isEmpty()) {
        return;
    }
    if (!savePath(path)) {
        QMessageBox::warning(this, tr("Save Failed"), lastError_);
    }
}

void MainWindow::closeCurrentDocument()
{
    closeDocumentAt(documents_->currentIndex());
}

void MainWindow::nextDocument()
{
    const int n = documents_->count();
    if (n > 1) {
        documents_->setCurrentIndex((documents_->currentIndex() + 1) % n);
    }
}

void MainWindow::previousDocument()
{
    const int n = documents_->count();
    if (n > 1) {
        documents_->setCurrentIndex((documents_->currentIndex() + n - 1) % n);
    }
}

void MainWindow::updateWindowTitle()
{
    const Document* document = documents_->current();
    const QString name = document != nullptr ? document->displayName() : tr("Untitled");
    const QString marker =
        (document != nullptr && document->isModified()) ? QStringLiteral("*") : QString();
    setWindowTitle(QStringLiteral("%1%2 — hungryeditor").arg(marker, name));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About hungryeditor"),
                       tr("<h3>hungryeditor %1</h3>"
                          "<p>A fast, native Markdown editor.</p>")
                           .arg(QStringLiteral(HUNGRYEDITOR_VERSION)));
}

} // namespace hungryeditor
