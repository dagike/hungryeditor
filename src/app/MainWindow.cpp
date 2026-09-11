#include "app/MainWindow.h"

#include <algorithm>
#include <functional>
#include <utility>

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "app/TabBar.h"
#include "editor/CaretHistory.h"
#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"
#include "export/HtmlDocument.h"
#include "io/AssetWriter.h"
#include "io/DraftStore.h"
#include "io/Preferences.h"
#include "io/RecentFiles.h"
#include "io/SessionStore.h"
#include "io/TextFile.h"
#include "markdown/Outline.h"
#include "preview/PreviewBackend.h"
#include "preview/PreviewController.h"
#include "preview/QtWebEnginePreview.h"
#include "theme/Theme.h"
#include "theme/ThemeFile.h"
#include "ui/CommandPalette.h"
#include "ui/FileTreePanel.h"
#include "ui/FindReplaceBar.h"
#include "ui/OutlinePanel.h"
#include "ui/PreferencesDialog.h"
#include "ui/SearchResultsPanel.h"
#include "ui/TabSwitcher.h"
#include "workspace/FileIndex.h"
#include "workspace/FileOperations.h"
#include "workspace/FileSearch.h"
#include "workspace/WorkspaceStore.h"

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

/// A whitespace-delimited word count of the raw buffer (markdown syntax
/// counted along with prose, same as most text editors' status bars).
int wordCount(const QString& text)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    const QString trimmed = text.trimmed();
    return trimmed.isEmpty() ? 0 : static_cast<int>(trimmed.split(whitespace).count());
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

    commandPalette_ = new CommandPalette(this);
    connect(commandPalette_, &CommandPalette::commandChosen, this, &MainWindow::runPaletteChoice);

    tabSwitcher_ = new TabSwitcher(this);
    connect(tabSwitcher_, &TabSwitcher::accepted, this, [this](int row) {
        if (row >= 0 && row < mruDocuments_.size()) {
            const int index = documents_->indexOf(mruDocuments_.at(row));
            if (index >= 0) {
                documents_->setCurrentIndex(index);
            }
        }
        editor_->setFocus();
    });
    connect(tabSwitcher_, &TabSwitcher::cancelled, this, [this] { editor_->setFocus(); });

    fileIndex_ = new FileIndex(this);
    connect(fileIndex_, &FileIndex::refreshed, this, [this] {
        if (paletteShowsFiles_ && !commandPalette_->isHidden()) {
            populateQuickOpen();
        }
        fileTree_->setFiles(fileIndex_->files());
    });

    searchResults_ = new SearchResultsPanel(this);
    searchDock_ = new QDockWidget(tr("Find in Files"), this);
    searchDock_->setObjectName(QStringLiteral("dock.searchResults"));
    searchDock_->setWidget(searchResults_);
    addDockWidget(Qt::BottomDockWidgetArea, searchDock_);
    searchDock_->hide();
    connect(searchResults_, &SearchResultsPanel::resultActivated, this,
            [this](const QString& path, int line) {
                recordCaretForHistory();
                if (openPath(path)) {
                    editor_->setCursorPosition(line, 0);
                }
            });

    outline_ = new OutlinePanel(this);
    outlineDock_ = new QDockWidget(tr("Outline"), this);
    outlineDock_->setObjectName(QStringLiteral("dock.outline"));
    outlineDock_->setWidget(outline_);
    addDockWidget(Qt::LeftDockWidgetArea, outlineDock_);
    outlineDock_->hide();
    connect(outline_, &OutlinePanel::headingActivated, this, [this](int line) {
        recordCaretForHistory();
        editor_->setCursorPosition(line, 0); // Scintilla scrolls the caret into view
        editor_->setFocus();
    });

    fileTree_ = new FileTreePanel(this);
    fileTree_->setMinimumWidth(160);
    fileTreeDock_ = new QDockWidget(tr("Files"), this);
    fileTreeDock_->setObjectName(QStringLiteral("dock.fileTree"));
    fileTreeDock_->setWidget(fileTree_);
    addDockWidget(Qt::LeftDockWidgetArea, fileTreeDock_);
    tabifyDockWidget(fileTreeDock_, outlineDock_);
    fileTreeDock_->hide();
    connect(fileTree_, &FileTreePanel::fileActivated, this, [this](const QString& path) {
        if (openPath(path)) {
            editor_->setFocus();
        }
    });
    connect(fileTree_, &FileTreePanel::createFileRequested, this, &MainWindow::promptCreateFile);
    connect(fileTree_, &FileTreePanel::createFolderRequested, this,
            &MainWindow::promptCreateFolder);
    connect(fileTree_, &FileTreePanel::renameRequested, this, &MainWindow::promptRename);
    connect(fileTree_, &FileTreePanel::deleteRequested, this, &MainWindow::promptDelete);

    outlineTimer_ = new QTimer(this);
    outlineTimer_->setSingleShot(true);
    outlineTimer_->setInterval(150);
    connect(outlineTimer_, &QTimer::timeout, this, &MainWindow::rebuildOutline);
    connect(outlineTimer_, &QTimer::timeout, this, &MainWindow::updateDocumentStatus);
    connect(editor_, &Editor::textChanged, this, [this] { outlineTimer_->start(); });
    connect(editor_, &Editor::cursorPositionChanged, this,
            [this](int line, int /*column*/) { outline_->highlightLine(line); });
    connect(editor_, &Editor::cursorPositionChanged, this, &MainWindow::updateCursorStatus);

    documents_ = std::make_unique<DocumentManager>(editor_);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { saveSession(); });

    preview_ = std::make_unique<QtWebEnginePreview>();
    previewController_ = std::make_unique<PreviewController>(preview_.get());
    QWidget* previewWidget = preview_->widget();
    previewWidget->setMinimumWidth(160);
    splitter_->addWidget(previewWidget);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    editor_->setImagePasteHandler([this](const QImage& image) -> QString {
        const QString ref = assets::writePastedImage(image, currentPath(), stateDir_);
        return ref.isEmpty() ? QString() : QStringLiteral("![](%1)").arg(ref);
    });

    connect(editor_, &Editor::textChanged, this, &MainWindow::refreshPreview);
    connect(editor_, &Editor::textChanged, this, &MainWindow::updateFrontMatterAction);
    connect(editor_, &Editor::viewportScrolled, this, &MainWindow::syncPreviewToEditor);
    connect(preview_.get(), &PreviewBackend::scrolledToSourceLine, this,
            &MainWindow::syncEditorToPreview);
    connect(preview_.get(), &PreviewBackend::clickedSourceLine, this,
            &MainWindow::jumpEditorToLine);
    connect(preview_.get(), &PreviewBackend::taskToggled, this,
            [this](int line, bool checked) { editor_->setTaskChecked(line, checked); });
    connect(preview_.get(), &PreviewBackend::ready, this, [this] { previewReady_ = true; });

    buildMenus();
    buildStatusBar();
    setStateDirectory(defaultStateDirectory());
    applyViewMode();
    setTheme(currentTheme_);

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

    QAction* openFolderAction =
        fileMenu->addAction(tr("Open &Folder…"), this, &MainWindow::openFolderDialog);
    openFolderAction->setObjectName(QStringLiteral("action.openFolder"));

    closeFolderAction_ =
        fileMenu->addAction(tr("Close Folder"), this, [this] { openFolder(QString()); });
    closeFolderAction_->setObjectName(QStringLiteral("action.closeFolder"));
    closeFolderAction_->setEnabled(false);

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

    QAction* nextTabAction =
        fileMenu->addAction(tr("Next &Recent Document"), this, [this] { quickSwitch(1); });
    nextTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
    nextTabAction->setObjectName(QStringLiteral("action.nextTab"));

    QAction* prevTabAction =
        fileMenu->addAction(tr("Previous Rece&nt Document"), this, [this] { quickSwitch(-1); });
    prevTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    prevTabAction->setObjectName(QStringLiteral("action.previousTab"));

    fileMenu->addSeparator();

    QMenu* exportMenu = fileMenu->addMenu(tr("&Export"));
    QAction* exportHtmlAction =
        exportMenu->addAction(tr("As &HTML…"), this, &MainWindow::exportHtmlDialog);
    exportHtmlAction->setObjectName(QStringLiteral("action.exportHtml"));

    QAction* printAction = exportMenu->addAction(tr("&Print…"), this, &MainWindow::printDialog);
    printAction->setObjectName(QStringLiteral("action.print"));

    QAction* exportPdfAction =
        exportMenu->addAction(tr("Export as &PDF…"), this, &MainWindow::exportPdfDialog);
    exportPdfAction->setObjectName(QStringLiteral("action.exportPdf"));

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

    QAction* findInFilesAction =
        editMenu->addAction(tr("Find in &Files…"), this, &MainWindow::findInFiles);
    findInFilesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    findInFilesAction->setObjectName(QStringLiteral("action.findInFiles"));

    editMenu->addSeparator();

    QAction* copyRichAction =
        editMenu->addAction(tr("Copy as &Rich Text"), this, &MainWindow::copyAsRichText);
    copyRichAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    copyRichAction->setObjectName(QStringLiteral("action.copyAsRichText"));

    editMenu->addSeparator();

    QAction* quickOpenAction =
        editMenu->addAction(tr("Go to &Anything…"), this, &MainWindow::openQuickOpen);
    quickOpenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    quickOpenAction->setObjectName(QStringLiteral("action.quickOpen"));

    QAction* paletteAction =
        editMenu->addAction(tr("Command &Palette…"), this, &MainWindow::openCommandPalette);
    paletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    paletteAction->setObjectName(QStringLiteral("action.commandPalette"));

    editMenu->addSeparator();

    QAction* goToLineAction =
        editMenu->addAction(tr("&Go to Line…"), this, &MainWindow::goToLineDialog);
    goToLineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    goToLineAction->setObjectName(QStringLiteral("action.goToLine"));

    QAction* backAction =
        editMenu->addAction(tr("Navigate &Back"), this, &MainWindow::navigateBack);
    backAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    backAction->setObjectName(QStringLiteral("action.navigateBack"));

    QAction* forwardAction =
        editMenu->addAction(tr("Navigate &Forward"), this, &MainWindow::navigateForward);
    forwardAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    forwardAction->setObjectName(QStringLiteral("action.navigateForward"));

    editMenu->addSeparator();

    QAction* selectNextAction = editMenu->addAction(tr("Select &Next Occurrence"), this,
                                                    [this] { editor_->selectNextOccurrence(); });
    selectNextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    selectNextAction->setObjectName(QStringLiteral("action.selectNext"));

    editMenu->addSeparator();

    const auto addLineAction = [&](const QString& text, const QString& objectName,
                                   const QKeySequence& shortcut, void (Editor::*op)()) {
        QAction* action = editMenu->addAction(text, this, [this, op] { (editor_->*op)(); });
        action->setShortcut(shortcut);
        action->setObjectName(objectName);
        return action;
    };
    addLineAction(tr("Move Line &Up"), QStringLiteral("action.moveLineUp"),
                  QKeySequence(Qt::ALT | Qt::Key_Up), &Editor::moveLinesUp);
    addLineAction(tr("Move Line &Down"), QStringLiteral("action.moveLineDown"),
                  QKeySequence(Qt::ALT | Qt::Key_Down), &Editor::moveLinesDown);
    addLineAction(tr("D&uplicate Line"), QStringLiteral("action.duplicateLine"),
                  QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), &Editor::duplicateSelection);
    addLineAction(tr("De&lete Line"), QStringLiteral("action.deleteLine"),
                  QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K), &Editor::deleteLines);
    addLineAction(tr("&Join Lines"), QStringLiteral("action.joinLines"),
                  QKeySequence(Qt::CTRL | Qt::Key_J), &Editor::joinLines);
    addLineAction(tr("Toggle &Comment"), QStringLiteral("action.toggleComment"),
                  QKeySequence(Qt::CTRL | Qt::Key_Slash), &Editor::toggleLineComment);

    editMenu->addSeparator();
    QAction* preferencesAction =
        editMenu->addAction(tr("&Preferences…"), this, &MainWindow::preferencesDialog);
    preferencesAction->setMenuRole(QAction::PreferencesRole);
    preferencesAction->setObjectName(QStringLiteral("action.preferences"));

    QMenu* formatMenu = menuBar()->addMenu(tr("F&ormat"));

    const auto addFormatAction = [&](const QString& text, const QString& objectName,
                                     const QKeySequence& shortcut, auto&& slot) {
        QAction* action = formatMenu->addAction(text, this, std::forward<decltype(slot)>(slot));
        action->setShortcut(shortcut);
        action->setObjectName(objectName);
        return action;
    };
    addFormatAction(tr("&Bold"), QStringLiteral("action.bold"), QKeySequence::Bold,
                    [this] { editor_->toggleInlineFormat(QStringLiteral("**")); });
    addFormatAction(tr("&Italic"), QStringLiteral("action.italic"), QKeySequence::Italic,
                    [this] { editor_->toggleInlineFormat(QStringLiteral("*")); });
    addFormatAction(tr("&Strikethrough"), QStringLiteral("action.strikethrough"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X),
                    [this] { editor_->toggleInlineFormat(QStringLiteral("~~")); });
    addFormatAction(tr("Inline &Code"), QStringLiteral("action.inlineCode"),
                    QKeySequence(Qt::CTRL | Qt::Key_E),
                    [this] { editor_->toggleInlineFormat(QStringLiteral("`")); });
    addFormatAction(tr("&Link…"), QStringLiteral("action.link"), QKeySequence(Qt::CTRL | Qt::Key_K),
                    [this] { editor_->insertLink(); });

    formatMenu->addSeparator();
    QMenu* headingMenu = formatMenu->addMenu(tr("&Heading"));
    for (int level = 1; level <= 6; ++level) {
        const auto key = static_cast<Qt::Key>(Qt::Key_0 + level);
        QAction* action = headingMenu->addAction(
            tr("Heading &%1").arg(level), this, [this, level] { editor_->setHeadingLevel(level); });
        action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | key));
        action->setObjectName(QStringLiteral("action.heading%1").arg(level));
    }
    QAction* paragraphAction =
        headingMenu->addAction(tr("&Paragraph"), this, [this] { editor_->setHeadingLevel(0); });
    paragraphAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_0));
    paragraphAction->setObjectName(QStringLiteral("action.headingParagraph"));

    formatMenu->addSeparator();
    addFormatAction(tr("Block&quote"), QStringLiteral("action.blockquote"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Period),
                    [this] { editor_->toggleBlockquote(); });
    addFormatAction(tr("&Bulleted List"), QStringLiteral("action.bulletList"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_8),
                    [this] { editor_->toggleBulletList(); });
    addFormatAction(tr("&Numbered List"), QStringLiteral("action.numberedList"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_7),
                    [this] { editor_->toggleNumberedList(); });

    formatMenu->addSeparator();
    addFormatAction(tr("Format &Table"), QStringLiteral("action.formatTable"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T),
                    [this] { editor_->formatTable(); });

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

    QMenu* themeMenu = viewMenu->addMenu(tr("&Theme"));
    themeGroup_ = new QActionGroup(this);
    const auto addTheme = [&](const QString& objectName, Theme::Builtin id) {
        QAction* action =
            themeMenu->addAction(Theme::builtinName(id), this, [this, id] { setTheme(id); });
        action->setCheckable(true);
        action->setObjectName(objectName);
        action->setData(static_cast<int>(id));
        themeGroup_->addAction(action);
    };
    addTheme(QStringLiteral("action.themeLight"), Theme::Builtin::Light);
    addTheme(QStringLiteral("action.themeDark"), Theme::Builtin::Dark);
    addTheme(QStringLiteral("action.themeHighContrast"), Theme::Builtin::HighContrast);
    addTheme(QStringLiteral("action.themeSepia"), Theme::Builtin::Sepia);

    themeMenu->addSeparator();
    QAction* loadCustomThemeAction =
        themeMenu->addAction(tr("Load Custom Theme…"), this, &MainWindow::loadCustomThemeDialog);
    loadCustomThemeAction->setObjectName(QStringLiteral("action.loadCustomTheme"));

    viewMenu->addSeparator();
    foldFrontMatterAction_ = viewMenu->addAction(tr("Fold &Front Matter"));
    foldFrontMatterAction_->setCheckable(true);
    foldFrontMatterAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Y));
    foldFrontMatterAction_->setObjectName(QStringLiteral("action.foldFrontMatter"));
    connect(foldFrontMatterAction_, &QAction::toggled, this,
            [this](bool on) { editor_->setFrontMatterFolded(on); });
    updateFrontMatterAction();

    QAction* filesAction = fileTreeDock_->toggleViewAction();
    filesAction->setText(tr("&Files"));
    filesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    filesAction->setObjectName(QStringLiteral("action.toggleFiles"));
    connect(filesAction, &QAction::toggled, this, [this](bool on) {
        if (on) {
            updateWorkspaceRoot();
        }
    });
    viewMenu->addAction(filesAction);

    QAction* outlineAction = outlineDock_->toggleViewAction();
    outlineAction->setText(tr("&Outline"));
    outlineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    outlineAction->setObjectName(QStringLiteral("action.toggleOutline"));
    viewMenu->addAction(outlineAction);

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
    if (Document* current = documents_->current()) {
        mruDocuments_.removeAll(current);
        mruDocuments_.prepend(current);
    }
    reconcileMru();

    syncingTabs_ = true;
    if (index >= 0 && index < tabBar_->count()) {
        tabBar_->setCurrentIndex(index);
    }
    syncingTabs_ = false;
    updateWindowTitle();

    // A tab switch replaces the buffer wholesale; push it now rather than
    // leaving the preview a debounce behind the visible document.
    previewController_->setDocumentPath(currentPath());
    if (viewMode_ != ViewMode::Editor) {
        previewController_->setMarkdown(editor_->text());
        previewController_->flush();
    }

    updateFrontMatterAction();
    rebuildOutline();
    updateWorkspaceRoot();
    updateCursorStatus(editor_->cursorLine(), editor_->cursorColumn());
    updateDocumentStatus();
}

void MainWindow::updateWorkspaceRoot()
{
    if (fileTreeDock_ == nullptr || !fileTreeDock_->toggleViewAction()->isChecked()) {
        return; // nothing scans until the sidebar is switched on
    }
    QString dir = workspaceRoot_;
    if (dir.isEmpty()) {
        const QString current = currentPath();
        dir = current.isEmpty() ? QDir::homePath() : QFileInfo(current).absolutePath();
    }
    fileTree_->setRoot(dir);
    fileIndex_->setRoot(dir);
    fileTree_->setFiles(fileIndex_->files()); // last scan now; refreshed() supplies the next
}

void MainWindow::openFolder(const QString& dir)
{
    const QString normalised = dir.isEmpty() ? QString() : QDir(dir).absolutePath();
    if (normalised == workspaceRoot_) {
        return;
    }

    saveWorkspaceViewState(); // remember the folder being left

    workspaceRoot_ = normalised;
    closeFolderAction_->setEnabled(!workspaceRoot_.isEmpty());
    if (!workspaceRoot_.isEmpty()) {
        fileTreeDock_->toggleViewAction()->setChecked(true); // reveal the sidebar
    }
    updateWorkspaceRoot();
    loadWorkspaceViewState();
    updateWindowTitle();
}

void MainWindow::openFolderDialog()
{
    const QString current = currentPath();
    const QString start = !workspaceRoot_.isEmpty() ? workspaceRoot_
                          : current.isEmpty()       ? QDir::homePath()
                                                    : QFileInfo(current).absolutePath();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"), start);
    if (!dir.isEmpty()) {
        openFolder(dir);
    }
}

void MainWindow::loadWorkspaceViewState()
{
    if (workspaceRoot_.isEmpty() || workspaceStore_ == nullptr) {
        return;
    }
    const WorkspaceState state = workspaceStore_->load(workspaceRoot_);
    fileTree_->applyState(state.filter, state.expandedDirs, state.hasExpandedList);
}

void MainWindow::saveWorkspaceViewState()
{
    if (workspaceRoot_.isEmpty() || workspaceStore_ == nullptr) {
        return;
    }
    WorkspaceState state;
    state.filter = fileTree_->filterText();
    state.expandedDirs = fileTree_->expandedDirectories();
    state.hasExpandedList = true;
    workspaceStore_->save(workspaceRoot_, state);
}

QList<int> MainWindow::documentsAffectedBy(const QString& path) const
{
    const QFileInfo info(path);
    const QString target = info.absoluteFilePath();
    const QString prefix = target + QLatin1Char('/');
    const bool isDir = info.isDir();

    QList<int> hits;
    for (int i = 0; i < documents_->count(); ++i) {
        const QString docPath = documents_->documentAt(i)->path();
        if (docPath.isEmpty()) {
            continue;
        }
        const QString resolved = QFileInfo(docPath).absoluteFilePath();
        if (resolved == target || (isDir && resolved.startsWith(prefix))) {
            hits.append(i);
        }
    }
    return hits;
}

bool MainWindow::createFileInWorkspace(const QString& parentDir, const QString& name)
{
    const fileops::Result result = fileops::createFile(parentDir, name);
    if (!result.ok) {
        lastError_ = result.error;
        return false;
    }
    fileIndex_->refresh();
    openPath(result.path);
    return true;
}

bool MainWindow::createFolderInWorkspace(const QString& parentDir, const QString& name)
{
    const fileops::Result result = fileops::createFolder(parentDir, name);
    if (!result.ok) {
        lastError_ = result.error;
        return false;
    }
    fileIndex_->refresh();
    return true;
}

bool MainWindow::renameInWorkspace(const QString& path, const QString& newName)
{
    QList<int> affected = documentsAffectedBy(path);
    for (int index : affected) {
        if (documents_->documentAt(index)->isModified()) {
            lastError_ =
                tr("Save your changes before renaming “%1”.").arg(QFileInfo(path).fileName());
            return false;
        }
    }

    const bool renamingCurrent =
        !affected.isEmpty() && affected.contains(documents_->currentIndex());

    const fileops::Result result = fileops::rename(path, newName);
    if (!result.ok) {
        lastError_ = result.error;
        return false;
    }

    // Close the now-stale buffers, highest index first so the rest stay valid.
    std::sort(affected.begin(), affected.end(), std::greater<>());
    for (int index : affected) {
        documents_->closeDocument(index);
    }
    if (renamingCurrent && QFileInfo(result.path).isFile()) {
        openPath(result.path);
    }
    fileIndex_->refresh();
    return true;
}

bool MainWindow::deleteFromWorkspace(const QString& path)
{
    QList<int> affected = documentsAffectedBy(path);
    for (int index : affected) {
        if (documents_->documentAt(index)->isModified()) {
            lastError_ =
                tr("Save your changes before deleting “%1”.").arg(QFileInfo(path).fileName());
            return false;
        }
    }

    const fileops::Result result = fileops::moveToTrash(path);
    if (!result.ok) {
        lastError_ = result.error;
        return false;
    }

    std::sort(affected.begin(), affected.end(), std::greater<>());
    for (int index : affected) {
        documents_->closeDocument(index);
    }
    fileIndex_->refresh();
    return true;
}

void MainWindow::promptCreateFile(const QString& parentDir)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New File"), tr("File name:"),
                                               QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty() && !createFileInWorkspace(parentDir, name)) {
        QMessageBox::warning(this, tr("New File"), lastError_);
    }
}

void MainWindow::promptCreateFolder(const QString& parentDir)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"),
                                               QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty() && !createFolderInWorkspace(parentDir, name)) {
        QMessageBox::warning(this, tr("New Folder"), lastError_);
    }
}

void MainWindow::promptRename(const QString& path, bool /*isDirectory*/)
{
    bool ok = false;
    const QString current = QFileInfo(path).fileName();
    const QString name =
        QInputDialog::getText(this, tr("Rename"), tr("New name:"), QLineEdit::Normal, current, &ok);
    if (ok && !name.isEmpty() && name != current && !renameInWorkspace(path, name)) {
        QMessageBox::warning(this, tr("Rename"), lastError_);
    }
}

void MainWindow::promptDelete(const QString& path, bool isDirectory)
{
    const QString name = QFileInfo(path).fileName();
    const QString question = isDirectory
                                 ? tr("Delete the folder “%1” and everything in it?").arg(name)
                                 : tr("Delete “%1”?").arg(name);
    if (QMessageBox::question(this, tr("Delete"), question) != QMessageBox::Yes) {
        return;
    }
    if (!deleteFromWorkspace(path)) {
        QMessageBox::warning(this, tr("Delete"), lastError_);
    }
}

void MainWindow::rebuildOutline()
{
    outline_->setHeadings(outline::parse(editor_->text()));
    outline_->highlightLine(editor_->cursorLine());
}

void MainWindow::buildStatusBar()
{
    statusPosition_ = new QLabel(this);
    statusCounts_ = new QLabel(this);
    statusLineEnding_ = new QLabel(this);
    statusEncoding_ = new QLabel(this);
    for (QLabel* label : {statusPosition_, statusCounts_, statusLineEnding_, statusEncoding_}) {
        label->setContentsMargins(6, 0, 6, 0);
        statusBar()->addPermanentWidget(label);
    }
    updateCursorStatus(editor_->cursorLine(), editor_->cursorColumn());
    updateDocumentStatus();
}

void MainWindow::updateCursorStatus(int line, int column)
{
    const int selected = static_cast<int>(editor_->selectedText().size());
    statusPosition_->setText(selected > 0 ? tr("Ln %1, Col %2 (%3 selected)")
                                                .arg(line + 1)
                                                .arg(column + 1)
                                                .arg(QLocale().toString(selected))
                                          : tr("Ln %1, Col %2").arg(line + 1).arg(column + 1));
}

void MainWindow::updateDocumentStatus()
{
    const QLocale locale;
    const QString text = editor_->text();
    statusCounts_->setText(
        tr("%1 words, %2 chars")
            .arg(locale.toString(wordCount(text)), locale.toString(static_cast<int>(text.size()))));

    const Document* current = documents_ ? documents_->current() : nullptr;
    statusLineEnding_->setText(
        lineEndingLabel(current != nullptr ? current->lineEnding() : LineEnding::Lf));
    statusEncoding_->setText(
        encodingLabel(current != nullptr ? current->encoding() : Encoding::Utf8));
}

QString MainWindow::statusPositionText() const
{
    return statusPosition_ != nullptr ? statusPosition_->text() : QString();
}

QString MainWindow::statusCountsText() const
{
    return statusCounts_ != nullptr ? statusCounts_->text() : QString();
}

QString MainWindow::statusLineEndingText() const
{
    return statusLineEnding_ != nullptr ? statusLineEnding_->text() : QString();
}

QString MainWindow::statusEncodingText() const
{
    return statusEncoding_ != nullptr ? statusEncoding_->text() : QString();
}

void MainWindow::updateFrontMatterAction()
{
    if (foldFrontMatterAction_ == nullptr) {
        return;
    }
    const bool has = editor_->hasFrontMatter();
    foldFrontMatterAction_->setEnabled(has);
    const QSignalBlocker block(foldFrontMatterAction_);
    foldFrontMatterAction_->setChecked(has && editor_->isFrontMatterFolded());
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
        previewController_->setDocumentPath(currentPath());
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

void MainWindow::setTheme(Theme::Builtin id)
{
    currentTheme_ = id;
    customThemePath_.clear();
    customThemeCss_.clear();
    const Theme theme = Theme::forBuiltin(id);
    preview_->setThemeCss(theme.previewCss());
    editor_->setTheme(theme);

    if (themeGroup_ != nullptr) {
        for (QAction* action : themeGroup_->actions()) {
            action->setChecked(action->data().toInt() == static_cast<int>(id));
        }
    }
}

bool MainWindow::loadCustomTheme(const QString& path)
{
    const themefile::Result result = themefile::loadThemeFile(path);
    if (!result.ok) {
        lastError_ = result.error;
        return false;
    }

    customThemePath_ = path;
    customTheme_ = result.theme;
    customThemeCss_ = result.customCss;
    preview_->setThemeCss(customTheme_.previewCss() + customThemeCss_);
    editor_->setTheme(customTheme_);

    if (themeGroup_ != nullptr) {
        for (QAction* action : themeGroup_->actions()) {
            action->setChecked(false);
        }
    }
    return true;
}

void MainWindow::loadCustomThemeDialog()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Load Custom Theme"), QString(),
                                                      tr("Theme Files (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    if (!loadCustomTheme(path)) {
        QMessageBox::warning(this, tr("Load Theme Failed"), lastError_);
    }
}

void MainWindow::applyPreferences()
{
    QFont font = preferences_.fontFamily.isEmpty()
                     ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                     : QFont(preferences_.fontFamily);
    font.setPointSize(preferences_.fontSize);
    editor_->setEditorFont(font);
    editor_->setTabWidth(preferences_.tabWidth);
    editor_->setWordWrap(preferences_.wordWrap);
}

void MainWindow::setPreferences(const Preferences& preferences)
{
    preferences_ = preferences;
    applyPreferences();
    if (preferencesStore_) {
        preferencesStore_->save(preferences_);
    }
}

void MainWindow::preferencesDialog()
{
    PreferencesDialog dialog(this);
    dialog.setPreferences(preferences_);
    if (dialog.exec() == QDialog::Accepted) {
        setPreferences(dialog.preferences());
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
    recordCaretForHistory();
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

void MainWindow::openCommandPalette()
{
    paletteShowsFiles_ = false;
    QList<CommandPalette::Command> commands;
    for (QAction* action : findChildren<QAction*>()) {
        const QString id = action->objectName();
        if (!id.startsWith(QLatin1String("action.")) || action->text().isEmpty() ||
            !action->isEnabled()) {
            continue;
        }
        QString title = action->text();
        title.remove(QLatin1Char('&'));
        if (title.endsWith(QChar(0x2026))) { // trailing ellipsis
            title.chop(1);
        }
        commands.append({id, title, action->shortcut().toString(QKeySequence::NativeText)});
    }
    commandPalette_->setCommands(commands);
    commandPalette_->open();
}

void MainWindow::openQuickOpen()
{
    paletteShowsFiles_ = true;
    const QString current = currentPath();
    const QString root = !workspaceRoot_.isEmpty() ? workspaceRoot_
                         : current.isEmpty()       ? QDir::homePath()
                                                   : QFileInfo(current).absolutePath();
    fileIndex_->setRoot(root);
    populateQuickOpen();
    commandPalette_->open();
}

void MainWindow::populateQuickOpen()
{
    reconcileMru();

    const QDir root(fileIndex_->root());
    QList<CommandPalette::Command> commands;
    QSet<QString> listed; // absolute paths already shown as open buffers

    // Open buffers first, in most-recently-used order.
    for (Document* document : mruDocuments_) {
        const QString path = document->path();
        if (path.isEmpty()) {
            commands.append(
                {QStringLiteral("buffer:") + QString::number(documents_->indexOf(document)),
                 document->displayName(), tr("open")});
        } else {
            listed.insert(QFileInfo(path).absoluteFilePath());
            commands.append({path, document->displayName(), tr("open")});
        }
    }

    for (const QString& path : fileIndex_->files()) {
        if (listed.contains(QFileInfo(path).absoluteFilePath())) {
            continue;
        }
        commands.append({path, root.relativeFilePath(path), QString()});
    }
    commandPalette_->setCommands(commands);
}

void MainWindow::runPaletteChoice(const QString& id)
{
    if (id.startsWith(QLatin1String("action."))) {
        if (QAction* action = findChild<QAction*>(id)) {
            action->trigger();
        }
    } else if (id.startsWith(QLatin1String("buffer:"))) {
        bool ok = false;
        const int index = id.mid(7).toInt(&ok);
        if (ok) {
            recordCaretForHistory();
            documents_->setCurrentIndex(index);
        }
    } else {
        recordCaretForHistory();
        openPath(id);
    }
}

void MainWindow::recordCaretForHistory()
{
    if (!navigatingHistory_) {
        caretHistory_.record(currentLocation());
    }
}

CaretLocation MainWindow::currentLocation() const
{
    return {currentPath(), editor_->cursorLine(), editor_->cursorColumn()};
}

void MainWindow::applyLocation(const CaretLocation& location)
{
    if (!location.path.isEmpty() && location.path != currentPath() && !openPath(location.path)) {
        return; // the file is gone — leave the caret where it is
    }
    editor_->setCursorPosition(location.line, location.column);
    editor_->setFocus();
}

void MainWindow::goToLine(int oneBasedLine)
{
    const int lastLine = qMax(1, editor_->lineCount());
    recordCaretForHistory();
    editor_->setCursorPosition(qBound(1, oneBasedLine, lastLine) - 1, 0);
    editor_->setFocus();
}

void MainWindow::goToLineDialog()
{
    bool ok = false;
    const int line =
        QInputDialog::getInt(this, tr("Go to Line"), tr("Line:"), editor_->cursorLine() + 1, 1,
                             qMax(1, editor_->lineCount()), 1, &ok);
    if (ok) {
        goToLine(line);
    }
}

void MainWindow::navigateBack()
{
    if (!caretHistory_.canGoBack()) {
        return;
    }
    navigatingHistory_ = true;
    applyLocation(caretHistory_.goBack(currentLocation()));
    navigatingHistory_ = false;
}

void MainWindow::navigateForward()
{
    if (!caretHistory_.canGoForward()) {
        return;
    }
    navigatingHistory_ = true;
    applyLocation(caretHistory_.goForward(currentLocation()));
    navigatingHistory_ = false;
}

void MainWindow::reconcileMru()
{
    for (auto it = mruDocuments_.begin(); it != mruDocuments_.end();) {
        if (documents_->indexOf(*it) < 0) {
            it = mruDocuments_.erase(it);
        } else {
            ++it;
        }
    }
    for (int i = 0; i < documents_->count(); ++i) {
        Document* document = documents_->documentAt(i);
        if (!mruDocuments_.contains(document)) {
            mruDocuments_.append(document);
        }
    }
}

QStringList MainWindow::mruDocumentNames() const
{
    QStringList names;
    for (Document* document : mruDocuments_) {
        if (documents_->indexOf(document) >= 0) {
            names.append(document->displayName());
        }
    }
    return names;
}

void MainWindow::quickSwitch(int direction)
{
    if (tabSwitcher_->isActive()) {
        if (direction < 0) {
            tabSwitcher_->selectPrevious();
        } else {
            tabSwitcher_->selectNext();
        }
        return;
    }

    reconcileMru();
    if (mruDocuments_.size() < 2) {
        return;
    }

    QList<TabSwitcher::Entry> entries;
    for (Document* document : mruDocuments_) {
        entries.append({document->displayName(),
                        document->isUntitled()
                            ? QString()
                            : QDir::toNativeSeparators(QFileInfo(document->path()).absolutePath()),
                        document->isModified()});
    }
    tabSwitcher_->present(entries, direction < 0 ? static_cast<int>(entries.size()) - 1 : 1);
}

void MainWindow::findInFiles()
{
    const QString current = currentPath();
    const QString directory =
        current.isEmpty() ? QDir::homePath() : QFileInfo(current).absolutePath();

    bool accepted = false;
    const QString query = QInputDialog::getText(
        this, tr("Find in Files"), tr("Search %1 for:").arg(QDir::toNativeSeparators(directory)),
        QLineEdit::Normal, editor_->selectedText(), &accepted);
    if (!accepted || query.isEmpty()) {
        return;
    }

    const QList<FileSearchHit> hits = searchDirectory(directory, query, {});
    searchResults_->showResults(query, hits);
    searchDock_->show();
    searchDock_->raise();
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
    stateDir_ = directory;
    documents_->setDraftDirectory(directory + QLatin1String("/drafts"));
    sessionStore_ = std::make_unique<SessionStore>(directory + QLatin1String("/session.json"));
    workspaceStore_ =
        std::make_unique<WorkspaceStore>(directory + QLatin1String("/workspaces.json"));
    recentFiles_ = std::make_unique<RecentFiles>(directory + QLatin1String("/recent.json"));
    refreshRecentFilesMenu();

    preferencesStore_ =
        std::make_unique<PreferencesStore>(directory + QLatin1String("/preferences.json"));
    preferences_ = preferencesStore_->load();
    applyPreferences();
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
    if (!session.windowState.isEmpty()) {
        restoreState(session.windowState);
    }
    if (!session.splitterState.isEmpty()) {
        splitter_->restoreState(session.splitterState);
    }
    if (!session.theme.isEmpty()) {
        setTheme(Theme::builtinFromKey(session.theme, currentTheme_));
    }
    if (!session.customThemePath.isEmpty()) {
        loadCustomTheme(session.customThemePath); // silently keeps the builtin above on failure
    }
    documents_->restoreSession(session, documents_->pendingDrafts());
    dropInitialBlankBuffer();
    if (session.currentIndex >= 0 && session.currentIndex < documents_->count()) {
        documents_->setCurrentIndex(session.currentIndex);
    }
    rebuildOutline();
    if (!session.workspaceFolder.isEmpty()) {
        openFolder(session.workspaceFolder); // re-roots, scans, re-checks the dock
    } else if (fileTreeDock_->toggleViewAction()->isChecked()) {
        updateWorkspaceRoot(); // restoreState revealed the sidebar — kick the scan
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
    session.windowState = saveState();
    session.splitterState = splitter_->saveState();
    session.workspaceFolder = workspaceRoot_;
    session.theme = Theme::builtinKey(currentTheme_);
    session.customThemePath = customThemePath_;
    saveWorkspaceViewState();
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
    previewController_->setDocumentPath(document->path()); // a Save As re-anchors images
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

QString MainWindow::exportTitle() const
{
    const QVector<outline::Heading> headings = outline::parse(editor_->text());
    if (!headings.isEmpty()) {
        return headings.first().text;
    }
    const Document* document = documents_->current();
    if (document == nullptr || document->isUntitled()) {
        return tr("Untitled");
    }
    return QFileInfo(document->displayName()).completeBaseName();
}

QString MainWindow::buildHtmlExport() const
{
    const QString current = currentPath();
    const htmlexport::Options options{
        exportTitle(), current.isEmpty() ? QString() : QFileInfo(current).absolutePath(),
        currentTheme()};
    return htmlexport::build(editor_->text(), options);
}

bool MainWindow::exportHtmlTo(const QString& path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        lastError_ = file.errorString();
        return false;
    }
    file.write(buildHtmlExport().toUtf8());
    if (!file.commit()) {
        lastError_ = file.errorString();
        return false;
    }
    lastError_.clear();
    return true;
}

void MainWindow::exportHtmlDialog()
{
    const QFileInfo current(currentPath());
    const QString dir = current.exists() ? current.absolutePath() : QDir::homePath();
    const QString suggested =
        QDir(dir).filePath(exportTitle().isEmpty() ? QStringLiteral("export") : exportTitle()) +
        QStringLiteral(".html");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export as HTML"), suggested,
                                                      tr("HTML files (*.html)"));
    if (path.isEmpty()) {
        return;
    }
    if (!exportHtmlTo(path)) {
        QMessageBox::warning(this, tr("Export Failed"), lastError_);
    }
}

void MainWindow::ensurePreviewRendered()
{
    previewController_->setMarkdown(editor_->text());
    previewController_->flush();
    if (previewReady_) {
        return;
    }
    QEventLoop loop;
    const QMetaObject::Connection connection =
        connect(preview_.get(), &PreviewBackend::ready, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(connection);
}

bool MainWindow::exportPdfTo(const QString& path)
{
    ensurePreviewRendered();
    if (!preview_->printToPdf(path)) {
        lastError_ = tr("Could not write PDF.");
        return false;
    }
    lastError_.clear();
    return true;
}

void MainWindow::printDialog()
{
    ensurePreviewRendered();
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (!preview_->print(&printer)) {
        QMessageBox::warning(this, tr("Print Failed"), tr("The document could not be printed."));
    }
}

void MainWindow::exportPdfDialog()
{
    const QFileInfo current(currentPath());
    const QString dir = current.exists() ? current.absolutePath() : QDir::homePath();
    const QString suggested =
        QDir(dir).filePath(exportTitle().isEmpty() ? QStringLiteral("export") : exportTitle()) +
        QStringLiteral(".pdf");
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Export as PDF"), suggested, tr("PDF files (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    if (!exportPdfTo(path)) {
        QMessageBox::warning(this, tr("Export Failed"), lastError_);
    }
}

void MainWindow::copyAsRichText()
{
    const QString selected = editor_->selectedText();
    const QString source = selected.isEmpty() ? editor_->text() : selected;
    const QString current = currentPath();
    const QString dir = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();

    auto* mime = new QMimeData();
    mime->setHtml(htmlexport::buildClipboardFragment(source, dir, currentTheme()));
    mime->setText(source);
    QApplication::clipboard()->setMimeData(mime);
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
    QString title = QStringLiteral("%1%2 — hungryeditor").arg(marker, name);
    if (!workspaceRoot_.isEmpty()) {
        title += QStringLiteral(" [%1]").arg(QDir(workspaceRoot_).dirName());
    }
    setWindowTitle(title);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About hungryeditor"),
                       tr("<h3>hungryeditor %1</h3>"
                          "<p>A fast, native Markdown editor.</p>")
                           .arg(QStringLiteral(HUNGRYEDITOR_VERSION)));
}

} // namespace hungryeditor
