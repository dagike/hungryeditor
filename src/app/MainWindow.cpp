#include "app/MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "app/TabBar.h"
#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"
#include "io/DraftStore.h"
#include "io/RecentFiles.h"
#include "io/SessionStore.h"
#include "preview/PreviewBackend.h"
#include "preview/PreviewController.h"
#include "preview/QtWebEnginePreview.h"
#include "theme/Theme.h"
#include "ui/FindReplaceBar.h"

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

/// Base directory for this app's persisted state — recovery drafts and the
/// session file. Falls back to a temp path when the platform offers none.
QString defaultStateDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath() + QLatin1String("/hungryeditor");
    }
    return base;
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
    findBar_ = new FindReplaceBar(container);
    splitter_ = new QSplitter(Qt::Horizontal, container);
    splitter_->setChildrenCollapsible(false);
    splitter_->addWidget(editor_);
    layout->addWidget(tabBar_);
    layout->addWidget(findBar_);
    layout->addWidget(splitter_);
    setCentralWidget(container);

    connect(findBar_, &FindReplaceBar::findRequested, this, [this](bool forward) {
        editor_->findNext(findBar_->query(), findBar_->options(), forward);
        refreshFindHighlight();
    });
    connect(findBar_, &FindReplaceBar::replaceOneRequested, this, [this] {
        editor_->replaceCurrent(findBar_->query(), findBar_->replacement(), findBar_->options());
        refreshFindHighlight();
    });
    connect(findBar_, &FindReplaceBar::replaceAllRequested, this, [this] {
        editor_->replaceAll(findBar_->query(), findBar_->replacement(), findBar_->options());
        refreshFindHighlight();
    });
    connect(findBar_, &FindReplaceBar::queryChanged, this, &MainWindow::refreshFindHighlight);
    connect(findBar_, &FindReplaceBar::dismissed, this, &MainWindow::closeFindBar);

    documents_ = std::make_unique<DocumentManager>(editor_);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { saveSession(); });

    preview_ = std::make_unique<QtWebEnginePreview>();
    previewController_ = std::make_unique<PreviewController>(preview_.get());
    preview_->setThemeCss(Theme::builtin().previewCss());
    QWidget* previewWidget = preview_->widget();
    previewWidget->setMinimumWidth(160);
    splitter_->addWidget(previewWidget);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    connect(editor_, &Editor::textChanged, this, &MainWindow::refreshPreview);
    connect(editor_, &Editor::viewportScrolled, this, &MainWindow::syncPreviewToEditor);
    connect(preview_.get(), &PreviewBackend::scrolledToSourceLine, this,
            &MainWindow::syncEditorToPreview);
    connect(preview_.get(), &PreviewBackend::clickedSourceLine, this,
            &MainWindow::jumpEditorToLine);

    buildMenus();
    setStateDirectory(defaultStateDirectory());
    applyViewMode();

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

    recentMenu_ = fileMenu->addMenu(tr("Open &Recent"));
    recentMenu_->setObjectName(QStringLiteral("menu.openRecent"));
    recentMenu_->menuAction()->setObjectName(QStringLiteral("action.openRecent"));
    connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::refreshRecentFilesMenu);

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

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));

    const auto openFindBar = [this](bool withReplace) {
        const QString selected = editor_->selectedText();
        if (!selected.isEmpty() && !selected.contains(QLatin1Char('\n'))) {
            findBar_->setQuery(selected);
        }
        findBar_->reveal(withReplace);
        refreshFindHighlight();
    };

    QAction* findAction =
        editMenu->addAction(tr("&Find…"), this, [openFindBar] { openFindBar(false); });
    findAction->setShortcut(QKeySequence::Find);
    findAction->setObjectName(QStringLiteral("action.find"));

    QAction* replaceAction =
        editMenu->addAction(tr("&Replace…"), this, [openFindBar] { openFindBar(true); });
    replaceAction->setShortcut(QKeySequence::Replace);
    replaceAction->setObjectName(QStringLiteral("action.replace"));

    editMenu->addSeparator();

    QAction* selectNextAction = editMenu->addAction(tr("Select &Next Occurrence"), this,
                                                    [this] { editor_->selectNextOccurrence(); });
    selectNextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    selectNextAction->setObjectName(QStringLiteral("action.selectNext"));

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewModeGroup_ = new QActionGroup(this);

    const auto addViewMode = [&](const QString& text, const QString& objectName, ViewMode mode,
                                 const QKeySequence& shortcut) {
        QAction* action = viewMenu->addAction(text, this, [this, mode] { setViewMode(mode); });
        action->setCheckable(true);
        action->setShortcut(shortcut);
        action->setObjectName(objectName);
        action->setData(static_cast<int>(mode));
        viewModeGroup_->addAction(action);
        return action;
    };
    addViewMode(tr("&Editor Only"), QStringLiteral("action.viewEditor"), ViewMode::Editor,
                QKeySequence(Qt::CTRL | Qt::Key_1));
    addViewMode(tr("&Split"), QStringLiteral("action.viewSplit"), ViewMode::Split,
                QKeySequence(Qt::CTRL | Qt::Key_2));
    addViewMode(tr("&Preview Only"), QStringLiteral("action.viewPreview"), ViewMode::Preview,
                QKeySequence(Qt::CTRL | Qt::Key_3));

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

    // A tab switch replaces the buffer wholesale; push it now rather than
    // leaving the preview a debounce behind the visible document.
    if (viewMode_ != ViewMode::Editor) {
        previewController_->setMarkdown(editor_->text());
        previewController_->flush();
    }
}

QWidget* MainWindow::previewWidget() const
{
    return preview_ != nullptr ? preview_->widget() : nullptr;
}

void MainWindow::setViewMode(ViewMode mode)
{
    if (viewMode_ == mode) {
        return;
    }
    viewMode_ = mode;
    applyViewMode();

    // Newly revealed, the preview needs the current buffer straight away.
    if (viewMode_ != ViewMode::Editor) {
        previewController_->setMarkdown(editor_->text());
        previewController_->flush();
    }
}

void MainWindow::applyViewMode()
{
    editor_->setVisible(viewMode_ != ViewMode::Preview);
    preview_->widget()->setVisible(viewMode_ != ViewMode::Editor);

    if (viewModeGroup_ != nullptr) {
        for (QAction* action : viewModeGroup_->actions()) {
            if (action->data().toInt() == static_cast<int>(viewMode_)) {
                action->setChecked(true);
            }
        }
    }
}

void MainWindow::refreshPreview()
{
    if (viewMode_ != ViewMode::Editor) {
        previewController_->setMarkdown(editor_->text());
    }
}

void MainWindow::syncPreviewToEditor()
{
    if (syncingScroll_ || viewMode_ != ViewMode::Split) {
        return;
    }
    syncingScroll_ = true;
    preview_->scrollToSourceLine(editor_->firstVisibleLine());
    syncingScroll_ = false;
}

void MainWindow::syncEditorToPreview(int line)
{
    if (syncingScroll_ || viewMode_ != ViewMode::Split) {
        return;
    }
    syncingScroll_ = true;
    editor_->setFirstVisibleLine(line);
    syncingScroll_ = false;
}

void MainWindow::jumpEditorToLine(int line)
{
    editor_->setCursorPosition(line, 0); // Scintilla scrolls the caret into view
    if (viewMode_ == ViewMode::Split) {
        editor_->setFocus();
    }
}

void MainWindow::refreshFindHighlight()
{
    if (findBar_->isHidden()) {
        return;
    }
    findBar_->setMatchCount(editor_->markAllMatches(findBar_->query(), findBar_->options()));
}

void MainWindow::closeFindBar()
{
    findBar_->hide();
    editor_->markAllMatches(QString(), {});
    editor_->setFocus();
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
        } else {
            recordRecent(document->path());
            if (firstOpened == nullptr) {
                firstOpened = document;
            }
        }
    }

    if (firstOpened != nullptr) {
        documents_->setCurrentIndex(documents_->indexOf(firstOpened));
        dropInitialBlankBuffer();
    }

    lastError_ = failures.join(QLatin1Char('\n'));
    return failures.isEmpty();
}

void MainWindow::dropInitialBlankBuffer()
{
    // Drop the blank buffer the window starts with once real content has
    // arrived, so a command-line open, a drop or a draft restore does not
    // leave a stray "Untitled" tab behind.
    if (documents_->count() < 2) {
        return;
    }
    Document* first = documents_->documentAt(0);
    if (first->isUntitled() && !first->isModified()) {
        documents_->closeDocument(0);
    }
}

bool MainWindow::hasRecoverableDrafts() const
{
    return !documents_->pendingDrafts().isEmpty();
}

void MainWindow::restoreUnsavedFromLastSession(bool askFirst)
{
    const QList<Draft> drafts = documents_->pendingDrafts();
    if (drafts.isEmpty()) {
        return;
    }

    if (askFirst) {
        const auto choice = QMessageBox::question(
            this, tr("Restore Unsaved Work"),
            tr("hungryeditor closed with %n unsaved document(s). Restore them?", nullptr,
               static_cast<int>(drafts.size())),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (choice != QMessageBox::Yes) {
            documents_->clearDrafts();
            return;
        }
    }

    documents_->restoreDrafts(drafts);
    dropInitialBlankBuffer();
    updateWindowTitle();
}

void MainWindow::setStateDirectory(const QString& directory)
{
    documents_->setDraftDirectory(directory + QLatin1String("/drafts"));
    sessionStore_ = std::make_unique<SessionStore>(directory + QLatin1String("/session.json"));
    recentFiles_ = std::make_unique<RecentFiles>(directory + QLatin1String("/recent.json"));
    refreshRecentFilesMenu();
}

void MainWindow::refreshRecentFilesMenu()
{
    if (recentMenu_ == nullptr || recentFiles_ == nullptr) {
        return;
    }
    // Rebuilt only from aboutToShow (or a test), never from inside an action's
    // own handler — clearing the menu there would delete the running action.
    recentMenu_->clear();

    bool anyShown = false;
    for (const RecentFile& entry : recentFiles_->entries()) {
        if (!QFileInfo::exists(entry.path)) {
            continue; // keep it on disk, just do not offer a dead link
        }
        const QString name = QFileInfo(entry.path).fileName();
        const QString label =
            entry.pinned ? QString(QChar(0x2605)) + QLatin1String("  ") + name : name;
        QAction* action = recentMenu_->addAction(label);
        action->setData(entry.path);
        action->setToolTip(entry.path);
        const QString path = entry.path;
        connect(action, &QAction::triggered, this, [this, path] { openRecent(path); });
        anyShown = true;
    }
    if (!anyShown) {
        QAction* none = recentMenu_->addAction(tr("No Recent Files"));
        none->setEnabled(false);
    }

    recentMenu_->addSeparator();

    const QString current = currentPath();
    QAction* pin = recentMenu_->addAction(tr("Pin Current File"));
    pin->setObjectName(QStringLiteral("action.pinCurrentFile"));
    pin->setCheckable(true);
    pin->setEnabled(!current.isEmpty());
    pin->setChecked(!current.isEmpty() && recentFiles_->isPinned(current));
    connect(pin, &QAction::triggered, this, [this](bool checked) {
        const QString path = currentPath();
        if (!path.isEmpty()) {
            recentFiles_->setPinned(path, checked);
            recentFiles_->save();
        }
    });

    QAction* clear = recentMenu_->addAction(tr("Clear Recent Files"));
    clear->setObjectName(QStringLiteral("action.clearRecentFiles"));
    connect(clear, &QAction::triggered, this, [this] {
        recentFiles_->clearUnpinned();
        recentFiles_->save();
    });
}

void MainWindow::recordRecent(const QString& path)
{
    if (path.isEmpty() || recentFiles_ == nullptr) {
        return;
    }
    recentFiles_->noteOpened(path);
    recentFiles_->save();
}

void MainWindow::openRecent(const QString& path)
{
    if (openPath(path)) {
        return;
    }
    if (recentFiles_ != nullptr) {
        recentFiles_->forget(path);
        recentFiles_->save();
    }
    QMessageBox::warning(this, tr("Open Failed"), lastError_);
}

void MainWindow::restoreLastSession(bool askFirst)
{
    const Session session = sessionStore_ ? sessionStore_->load() : Session{};
    if (!session.valid) {
        // No clean-exit session — fall back to crash recovery.
        restoreUnsavedFromLastSession(askFirst);
        return;
    }

    if (!session.windowGeometry.isEmpty()) {
        restoreGeometry(session.windowGeometry);
    }
    documents_->restoreSession(session, documents_->pendingDrafts());
    dropInitialBlankBuffer();
    if (session.currentIndex >= 0 && session.currentIndex < documents_->count()) {
        documents_->setCurrentIndex(session.currentIndex);
    }
    sessionStore_->clear(); // consumed; only a crash should leave one behind
    updateWindowTitle();
}

void MainWindow::saveSession()
{
    if (!sessionStore_) {
        return;
    }
    documents_->autosaveDirtyDocuments(); // flush the latest text into drafts
    Session session = documents_->buildSession();
    session.windowGeometry = saveGeometry();
    sessionStore_->save(session);
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
    recordRecent(document->path());
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
