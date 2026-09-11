// Smoke coverage for the application window.

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QtTest>
#include <QUrl>

#include "app/MainWindow.h"
#include "app/TabBar.h"
#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"
#include "io/DraftStore.h"
#include "io/RecentFiles.h"
#include "io/SessionStore.h"
#include "preview/PreviewBackend.h"
#include "preview/PreviewController.h"
#include "ui/CommandPalette.h"
#include "ui/FileTreePanel.h"
#include "ui/FindReplaceBar.h"
#include "ui/OutlinePanel.h"
#include "ui/SearchResultsPanel.h"
#include "ui/TabSwitcher.h"
#include "workspace/FileSearch.h"

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void editorSitsBelowTheTabBar();
    void hasExpectedMenus();
    void hasNamedActions_data();
    void hasNamedActions();
    void showsWithoutCrashing();
    void saveActionFollowsDirtyState();
    void openPathLoadsFileAndClearsDirty();
    void savePathWritesBufferPreservingLineEnding();
    void exportHtmlWritesAStandaloneFile();
    void exportPdfWritesAFileEvenFromEditorOnlyView();
    void copyAsRichTextPutsHtmlAndPlainTextOnTheClipboard();
    void openPathReportsMissingFile();
    void openFilesOpensEachActivatingTheFirst();
    void openFilesReportsFailuresAndOpensTheRest();
    void droppedFilesOpenInTheEditor();
    void externalEditReloadsACleanBuffer();
    void externalEditDoesNotClobberADirtyBuffer();
    void restoresUnsavedDraftsOnStartup();
    void cleanShutdownWritesASession();
    void restoresTabsAndGeometryFromACleanSession();
    void sessionRestoreFallsBackToDraftRecovery();
    void recentFilesMenuFillsAsFilesOpen();
    void recentEntryReopensItsFile();
    void pinnedRecentSurvivesManyOpens();
    void clearRecentFilesKeepsPinned();
    void newAndSwitchActionsChangeCurrentDocument();
    void defaultsToSplitViewWithBothPanes();
    void viewModeActionsTogglePaneVisibility();
    void editorTextFlowsIntoThePreview();
    void switchingDocumentsRefreshesThePreview();
    void scrollSyncsBothWays();
    void clickingAPreviewHeadingMovesTheCaret();
    void clickingAPreviewCheckboxRewritesTheSource();
    void selectNextActionAddsACaret();
    void lineActionsEditTheBuffer();
    void formatActionsEditTheBuffer();
    void findBarSearchesAndReplaces();
    void activatingASearchResultOpensTheFile();
    void outlinePanelListsHeadingsAndJumpsToThem();
    void fileSidebarListsTheFolderAndOpensAFile();
    void openFolderRootsTheSidebarAndRevealsIt();
    void workspaceAndFilterSurviveASessionReload();
    void sidebarCreatesRenamesAndDeletes();
    void commandPaletteRunsTheChosenAction();
    void quickOpenOpensAFuzzilyMatchedFile();
    void mruOrderFollowsActivation();
    void quickSwitchWalksMruAndCommits();
    void goToAnythingListsOpenBuffersFirst();
    void goToLineMovesTheCaretWithinTheBuffer();
    void caretHistoryReturnsToPriorSpots();
    void panelLayoutSurvivesAReload();
};

namespace {

QString writeText(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(bytes);
    file.close();
    return path;
}

/// Paths of the file entries in the "Open Recent" submenu, top to bottom.
QStringList recentEntryPaths(const QMenu* menu)
{
    QStringList paths;
    for (const QAction* action : menu->actions()) {
        const QString path = action->data().toString();
        if (!path.isEmpty()) {
            paths << path;
        }
    }
    return paths;
}

} // namespace

void TestMainWindow::editorSitsBelowTheTabBar()
{
    hungryeditor::MainWindow window;
    QVERIFY(window.editor() != nullptr);
    QVERIFY(window.tabBar() != nullptr);
    QVERIFY(window.centralWidget()->isAncestorOf(window.editor()));
    QVERIFY(window.centralWidget()->isAncestorOf(window.tabBar()));
    // A single document keeps the tab strip hidden.
    QCOMPARE(window.documents()->count(), 1);
    QVERIFY(window.tabBar()->isHidden());
}

void TestMainWindow::hasExpectedMenus()
{
    hungryeditor::MainWindow window;
    QCOMPARE(window.menuBar()->actions().size(), 5); // File, Edit, Format, View, Help
}

void TestMainWindow::saveActionFollowsDirtyState()
{
    hungryeditor::MainWindow window;
    QAction* save = window.findChild<QAction*>(QStringLiteral("action.save"));
    QVERIFY(save != nullptr);
    QVERIFY(!save->isEnabled());

    window.editor()->setText(QStringLiteral("edited"));
    QVERIFY(save->isEnabled());

    window.editor()->markClean();
    QVERIFY(!save->isEnabled());
}

void TestMainWindow::openPathLoadsFileAndClearsDirty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("doc.md"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Loaded\n\nfrom disk\n");
    }

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(path));
    QCOMPARE(window.editor()->text(), QStringLiteral("# Loaded\n\nfrom disk\n"));
    QVERIFY(!window.editor()->isModified());
    QCOMPARE(window.currentPath(), path);
    QVERIFY(window.windowTitle().contains(QStringLiteral("doc.md")));
}

void TestMainWindow::savePathWritesBufferPreservingLineEnding()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("crlf.md"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("one\r\ntwo\r\n");
    }

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(source));
    window.editor()->setText(QStringLiteral("one\ntwo\nthree\n"));

    const QString target = dir.filePath(QStringLiteral("out.md"));
    QVERIFY(window.savePath(target));
    QVERIFY(!window.editor()->isModified());

    QFile written(target);
    QVERIFY(written.open(QIODevice::ReadOnly));
    QCOMPARE(written.readAll(), QByteArray("one\r\ntwo\r\nthree\r\n"));
}

void TestMainWindow::exportHtmlWritesAStandaloneFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("note.md"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Note\n\nSome *text*.\n");
    }

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(source));
    QCOMPARE(window.exportTitle(), QStringLiteral("Note"));

    const QString target = dir.filePath(QStringLiteral("note.html"));
    QVERIFY(window.exportHtmlTo(target));

    QFile written(target);
    QVERIFY(written.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(written.readAll());
    QVERIFY(html.startsWith(QLatin1String("<!doctype html>")));
    QVERIFY(html.contains(QLatin1String("<title>Note</title>")));
    QVERIFY(html.contains(QLatin1String("Some <em>text</em>")));
}

void TestMainWindow::exportPdfWritesAFileEvenFromEditorOnlyView()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("note.md"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Note\n\nSome text.\n");
    }

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(source));
    // Editor-only view never feeds the preview (refreshPreview() skips it),
    // so this also proves exportPdfTo() forces a render on its own.
    window.setViewMode(hungryeditor::MainWindow::ViewMode::Editor);

    const QString target = dir.filePath(QStringLiteral("note.pdf"));
    QVERIFY(window.exportPdfTo(target));
    QVERIFY(QFileInfo(target).size() > 0);
}

void TestMainWindow::copyAsRichTextPutsHtmlAndPlainTextOnTheClipboard()
{
    hungryeditor::MainWindow window;
    window.editor()->setText(QStringLiteral("# Title\n\nSome *text* here.\n"));

    window.findChild<QAction*>(QStringLiteral("action.copyAsRichText"))->trigger();
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    QVERIFY(mime->hasHtml());
    QVERIFY(mime->html().contains(QLatin1String("<h1")));
    QVERIFY(mime->html().contains(QLatin1String("Some <em>text</em>")));
    QCOMPARE(mime->text(), window.editor()->text());

    // With a selection, only the selection is copied.
    window.editor()->setCursorPosition(2, 6); // inside "text"
    window.editor()->selectNextOccurrence();
    QCOMPARE(window.editor()->selectedText(), QStringLiteral("text"));

    window.findChild<QAction*>(QStringLiteral("action.copyAsRichText"))->trigger();
    const QMimeData* selectionMime = QApplication::clipboard()->mimeData();
    QCOMPARE(selectionMime->text(), QStringLiteral("text"));
    QVERIFY(!selectionMime->html().contains(QLatin1String("<h1")));
}

void TestMainWindow::openPathReportsMissingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    hungryeditor::MainWindow window;
    QVERIFY(!window.openPath(dir.filePath(QStringLiteral("absent.md"))));
    QVERIFY(!window.lastError().isEmpty());
}

void TestMainWindow::openFilesOpensEachActivatingTheFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString first = dir.filePath(QStringLiteral("first.md"));
    const QString second = dir.filePath(QStringLiteral("second.md"));
    {
        QFile a(first);
        QVERIFY(a.open(QIODevice::WriteOnly));
        a.write("first\n");
        QFile b(second);
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.write("second\n");
    }

    hungryeditor::MainWindow window;
    QVERIFY(window.openFiles({first, second}));

    // The pristine untitled buffer is dropped, leaving just the two files.
    QCOMPARE(window.documents()->count(), 2);
    QCOMPARE(window.currentPath(), first);
    QCOMPARE(window.editor()->text(), QStringLiteral("first\n"));
}

void TestMainWindow::openFilesReportsFailuresAndOpensTheRest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString good = dir.filePath(QStringLiteral("good.md"));
    {
        QFile file(good);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("ok\n");
    }
    const QString missing = dir.filePath(QStringLiteral("missing.md"));

    hungryeditor::MainWindow window;
    QVERIFY(!window.openFiles({missing, good}));
    QVERIFY(window.lastError().contains(QStringLiteral("missing.md")));

    QCOMPARE(window.currentPath(), good);
    QCOMPARE(window.editor()->text(), QStringLiteral("ok\n"));
}

void TestMainWindow::droppedFilesOpenInTheEditor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("dropped.md"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("dropped content\n");
    }

    hungryeditor::MainWindow window;

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});

    QDragEnterEvent enter(QPoint(5, 5), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &enter);
    QVERIFY(enter.isAccepted());

    QDropEvent drop(QPointF(5, 5), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &drop);

    QCOMPARE(window.currentPath(), path);
    QCOMPARE(window.editor()->text(), QStringLiteral("dropped content\n"));
}

void TestMainWindow::externalEditReloadsACleanBuffer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeText(dir.filePath(QStringLiteral("live.md")), "before\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(path));
    QVERIFY(!window.editor()->isModified());

    writeText(path, "after the external edit\n");
    window.documents()->pollExternalChanges();

    QCOMPARE(window.editor()->text(), QStringLiteral("after the external edit\n"));
    QVERIFY(!window.editor()->isModified());
}

void TestMainWindow::externalEditDoesNotClobberADirtyBuffer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeText(dir.filePath(QStringLiteral("mine.md")), "disk original\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(path));
    window.editor()->setText(QStringLiteral("my unsaved edits\n"));
    QVERIFY(window.editor()->isModified());

    writeText(path, "disk changed underneath\n");
    window.documents()->pollExternalChanges();

    // The prompt is deferred; without an event loop turn the buffer is untouched.
    QCOMPARE(window.editor()->text(), QStringLiteral("my unsaved edits\n"));
    QVERIFY(window.editor()->isModified());

    // Choosing to reload explicitly replaces it.
    QVERIFY(window.reloadDocumentAt(window.documents()->currentIndex()));
    QCOMPARE(window.editor()->text(), QStringLiteral("disk changed underneath\n"));
    QVERIFY(!window.editor()->isModified());
}

void TestMainWindow::restoresUnsavedDraftsOnStartup()
{
    QTemporaryDir drafts;
    QVERIFY(drafts.isValid());
    {
        hungryeditor::DraftStore seed(drafts.path());
        hungryeditor::Draft draft;
        draft.id = QStringLiteral("restore-me");
        draft.text = QStringLiteral("half-written paragraph\n");
        QVERIFY(seed.write(draft));
    }

    hungryeditor::MainWindow window;
    window.documents()->setDraftDirectory(drafts.path());
    QVERIFY(window.hasRecoverableDrafts());

    window.restoreUnsavedFromLastSession(/*askFirst=*/false);

    QCOMPARE(window.documents()->count(), 1); // stray blank buffer dropped
    QCOMPARE(window.editor()->text(), QStringLiteral("half-written paragraph\n"));
    QVERIFY(window.editor()->isModified());
}

void TestMainWindow::cleanShutdownWritesASession()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    window.editor()->setText(QStringLiteral("unsaved edits\n"));

    window.saveSession(); // what the aboutToQuit hook runs

    hungryeditor::SessionStore store(state.path() + QStringLiteral("/session.json"));
    QVERIFY(store.load().valid);
    // The unsaved buffer stays recoverable through its draft.
    QVERIFY(
        !hungryeditor::DraftStore(state.path() + QStringLiteral("/drafts")).loadAll().isEmpty());
}

void TestMainWindow::restoresTabsAndGeometryFromACleanSession()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString a = writeText(files.filePath(QStringLiteral("a.md")), "alpha\n");
    const QString b = writeText(files.filePath(QStringLiteral("b.md")), "b0\nb1\nb2\n");

    {
        hungryeditor::MainWindow first;
        first.setStateDirectory(state.path());
        QVERIFY(first.openFiles({a, b}));
        first.documents()->setCurrentIndex(1); // user switches over to b
        first.editor()->setCursorPosition(2, 1);
        // Keep this within the headless 800x600 offscreen screen: restoreGeometry
        // clamps a larger window to the available screen area.
        first.resize(720, 480);
        first.saveSession();
    }

    hungryeditor::MainWindow second;
    second.setStateDirectory(state.path());
    second.restoreLastSession(/*askFirst=*/false);
    // Check the restored geometry before showing: a shown window can be resized
    // by the platform (the offscreen plugin on Windows trims the frame).
    QCOMPARE(second.size(), QSize(720, 480));
    second.show();
    QVERIFY(QTest::qWaitForWindowExposed(&second));

    QCOMPARE(second.documents()->count(), 2);
    QCOMPARE(second.currentPath(), b);
    QCOMPARE(second.editor()->text(), QStringLiteral("b0\nb1\nb2\n"));
    QCOMPARE(second.editor()->cursorLine(), 2);

    // The session file is consumed once restored.
    QVERIFY(
        !hungryeditor::SessionStore(state.path() + QStringLiteral("/session.json")).load().valid);
}

void TestMainWindow::sessionRestoreFallsBackToDraftRecovery()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    {
        hungryeditor::DraftStore seed(state.path() + QStringLiteral("/drafts"));
        hungryeditor::Draft draft;
        draft.id = QStringLiteral("crashed");
        draft.text = QStringLiteral("recovered text\n");
        QVERIFY(seed.write(draft));
    }

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    window.restoreLastSession(/*askFirst=*/false); // no session.json -> crash path

    QCOMPARE(window.documents()->count(), 1);
    QCOMPARE(window.editor()->text(), QStringLiteral("recovered text\n"));
    QVERIFY(window.editor()->isModified());
}

void TestMainWindow::recentFilesMenuFillsAsFilesOpen()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString a = writeText(files.filePath(QStringLiteral("a.md")), "a\n");
    const QString b = writeText(files.filePath(QStringLiteral("b.md")), "b\n");

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    QVERIFY(window.openPath(a));
    QVERIFY(window.openPath(b));

    window.refreshRecentFilesMenu();
    const QStringList paths = recentEntryPaths(window.recentFilesMenu());
    QCOMPARE(paths.size(), 2);
    QCOMPARE(paths.at(0), b); // most recent first
    QCOMPARE(paths.at(1), a);
}

void TestMainWindow::recentEntryReopensItsFile()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString a = writeText(files.filePath(QStringLiteral("a.md")), "aaa\n");
    const QString b = writeText(files.filePath(QStringLiteral("b.md")), "bbb\n");

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    QVERIFY(window.openPath(a));
    QVERIFY(window.openPath(b));
    QCOMPARE(window.currentPath(), b);

    window.refreshRecentFilesMenu();
    QAction* entryForA = nullptr;
    for (QAction* action : window.recentFilesMenu()->actions()) {
        if (action->data().toString() == a) {
            entryForA = action;
        }
    }
    QVERIFY(entryForA != nullptr);
    entryForA->trigger();

    QCOMPARE(window.currentPath(), a);
    QCOMPARE(window.editor()->text(), QStringLiteral("aaa\n"));
}

void TestMainWindow::pinnedRecentSurvivesManyOpens()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString keep = writeText(files.filePath(QStringLiteral("keep.md")), "keep\n");

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    QVERIFY(window.openPath(keep));

    window.refreshRecentFilesMenu();
    QAction* pin = window.findChild<QAction*>(QStringLiteral("action.pinCurrentFile"));
    QVERIFY(pin != nullptr);
    QVERIFY(pin->isEnabled());
    QVERIFY(!pin->isChecked());
    pin->trigger(); // pins keep.md

    for (int i = 0; i < hungryeditor::RecentFiles::kMaxRecent + 5; ++i) {
        const QString p = writeText(files.filePath(QStringLiteral("f%1.md").arg(i)), "x\n");
        QVERIFY(window.openPath(p));
    }

    window.refreshRecentFilesMenu();
    const QStringList paths = recentEntryPaths(window.recentFilesMenu());
    QVERIFY(paths.contains(keep));
    QCOMPARE(paths.first(), keep); // pinned entries lead the list
}

void TestMainWindow::clearRecentFilesKeepsPinned()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString a = writeText(files.filePath(QStringLiteral("a.md")), "a\n");
    const QString b = writeText(files.filePath(QStringLiteral("b.md")), "b\n");

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    QVERIFY(window.openPath(a));
    window.refreshRecentFilesMenu();
    window.findChild<QAction*>(QStringLiteral("action.pinCurrentFile"))->trigger(); // pin a
    QVERIFY(window.openPath(b));

    window.refreshRecentFilesMenu();
    window.findChild<QAction*>(QStringLiteral("action.clearRecentFiles"))->trigger();

    window.refreshRecentFilesMenu();
    const QStringList paths = recentEntryPaths(window.recentFilesMenu());
    QCOMPARE(paths.size(), 1);
    QCOMPARE(paths.first(), a);
}

void TestMainWindow::hasNamedActions_data()
{
    QTest::addColumn<QString>("objectName");
    QTest::newRow("new") << QStringLiteral("action.new");
    QTest::newRow("open") << QStringLiteral("action.open");
    QTest::newRow("save") << QStringLiteral("action.save");
    QTest::newRow("saveAs") << QStringLiteral("action.saveAs");
    QTest::newRow("close") << QStringLiteral("action.close");
    QTest::newRow("quit") << QStringLiteral("action.quit");
    QTest::newRow("about") << QStringLiteral("action.about");
    QTest::newRow("find") << QStringLiteral("action.find");
    QTest::newRow("replace") << QStringLiteral("action.replace");
    QTest::newRow("findInFiles") << QStringLiteral("action.findInFiles");
    QTest::newRow("quickOpen") << QStringLiteral("action.quickOpen");
    QTest::newRow("commandPalette") << QStringLiteral("action.commandPalette");
    QTest::newRow("selectNext") << QStringLiteral("action.selectNext");
    QTest::newRow("moveLineUp") << QStringLiteral("action.moveLineUp");
    QTest::newRow("duplicateLine") << QStringLiteral("action.duplicateLine");
    QTest::newRow("deleteLine") << QStringLiteral("action.deleteLine");
    QTest::newRow("joinLines") << QStringLiteral("action.joinLines");
    QTest::newRow("toggleComment") << QStringLiteral("action.toggleComment");
    QTest::newRow("bold") << QStringLiteral("action.bold");
    QTest::newRow("italic") << QStringLiteral("action.italic");
    QTest::newRow("strikethrough") << QStringLiteral("action.strikethrough");
    QTest::newRow("inlineCode") << QStringLiteral("action.inlineCode");
    QTest::newRow("link") << QStringLiteral("action.link");
    QTest::newRow("heading1") << QStringLiteral("action.heading1");
    QTest::newRow("heading6") << QStringLiteral("action.heading6");
    QTest::newRow("headingParagraph") << QStringLiteral("action.headingParagraph");
    QTest::newRow("blockquote") << QStringLiteral("action.blockquote");
    QTest::newRow("bulletList") << QStringLiteral("action.bulletList");
    QTest::newRow("numberedList") << QStringLiteral("action.numberedList");
    QTest::newRow("formatTable") << QStringLiteral("action.formatTable");
    QTest::newRow("foldFrontMatter") << QStringLiteral("action.foldFrontMatter");
    QTest::newRow("toggleOutline") << QStringLiteral("action.toggleOutline");
    QTest::newRow("toggleFiles") << QStringLiteral("action.toggleFiles");
    QTest::newRow("nextTab") << QStringLiteral("action.nextTab");
    QTest::newRow("previousTab") << QStringLiteral("action.previousTab");
    QTest::newRow("goToLine") << QStringLiteral("action.goToLine");
    QTest::newRow("navigateBack") << QStringLiteral("action.navigateBack");
    QTest::newRow("navigateForward") << QStringLiteral("action.navigateForward");
    QTest::newRow("openFolder") << QStringLiteral("action.openFolder");
    QTest::newRow("closeFolder") << QStringLiteral("action.closeFolder");
    QTest::newRow("viewEditor") << QStringLiteral("action.viewEditor");
    QTest::newRow("viewSplit") << QStringLiteral("action.viewSplit");
    QTest::newRow("viewPreview") << QStringLiteral("action.viewPreview");
    QTest::newRow("exportHtml") << QStringLiteral("action.exportHtml");
    QTest::newRow("print") << QStringLiteral("action.print");
    QTest::newRow("exportPdf") << QStringLiteral("action.exportPdf");
    QTest::newRow("copyAsRichText") << QStringLiteral("action.copyAsRichText");
}

void TestMainWindow::hasNamedActions()
{
    QFETCH(QString, objectName);
    hungryeditor::MainWindow window;
    QVERIFY(window.findChild<QAction*>(objectName) != nullptr);
}

void TestMainWindow::showsWithoutCrashing()
{
    hungryeditor::MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
}

void TestMainWindow::newAndSwitchActionsChangeCurrentDocument()
{
    hungryeditor::MainWindow window;
    QCOMPARE(window.documents()->count(), 1);

    window.findChild<QAction*>(QStringLiteral("action.new"))->trigger();
    QCOMPARE(window.documents()->count(), 2);
    QCOMPARE(window.documents()->currentIndex(), 1);

    window.findChild<QAction*>(QStringLiteral("action.previousDocument"))->trigger();
    QCOMPARE(window.documents()->currentIndex(), 0);

    window.findChild<QAction*>(QStringLiteral("action.nextDocument"))->trigger();
    QCOMPARE(window.documents()->currentIndex(), 1);
}

void TestMainWindow::defaultsToSplitViewWithBothPanes()
{
    hungryeditor::MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QCOMPARE(window.viewMode(), hungryeditor::MainWindow::ViewMode::Split);
    QVERIFY(window.editor()->isVisible());
    QVERIFY(window.previewWidget() != nullptr);
    QVERIFY(window.previewWidget()->isVisible());
    QVERIFY(window.findChild<QAction*>(QStringLiteral("action.viewSplit"))->isChecked());
}

void TestMainWindow::viewModeActionsTogglePaneVisibility()
{
    hungryeditor::MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.findChild<QAction*>(QStringLiteral("action.viewPreview"))->trigger();
    QCOMPARE(window.viewMode(), hungryeditor::MainWindow::ViewMode::Preview);
    QVERIFY(!window.editor()->isVisible());
    QVERIFY(window.previewWidget()->isVisible());

    window.findChild<QAction*>(QStringLiteral("action.viewEditor"))->trigger();
    QCOMPARE(window.viewMode(), hungryeditor::MainWindow::ViewMode::Editor);
    QVERIFY(window.editor()->isVisible());
    QVERIFY(!window.previewWidget()->isVisible());
}

void TestMainWindow::editorTextFlowsIntoThePreview()
{
    hungryeditor::MainWindow window;
    QSignalSpy rendered(window.previewController(), &hungryeditor::PreviewController::rendered);

    window.editor()->setText(QStringLiteral("# Live Heading\n\nsome prose\n"));

    QVERIFY(rendered.wait(2000));
    const QString html = rendered.last().at(0).toString();
    QVERIFY(html.contains(QStringLiteral(">Live Heading</h1>")));
    QVERIFY(html.contains(QStringLiteral(">some prose</p>")));
}

void TestMainWindow::switchingDocumentsRefreshesThePreview()
{
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString a = writeText(files.filePath(QStringLiteral("a.md")), "# Doc A\n");
    const QString b = writeText(files.filePath(QStringLiteral("b.md")), "# Doc B\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openFiles({a, b}));
    window.documents()->setCurrentIndex(1); // sitting on b

    QSignalSpy rendered(window.previewController(), &hungryeditor::PreviewController::rendered);
    window.documents()->setCurrentIndex(0); // switch back to a

    QVERIFY(rendered.count() >= 1 || rendered.wait(2000));
    QVERIFY(rendered.last().at(0).toString().contains(QStringLiteral(">Doc A</h1>")));
}

void TestMainWindow::scrollSyncsBothWays()
{
    hungryeditor::MainWindow window;
    window.resize(720, 320);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QString doc;
    for (int i = 0; i < 80; ++i) {
        doc += QStringLiteral("Paragraph %1 with a comfortable amount of filler text.\n\n").arg(i);
    }
    QSignalSpy ready(window.previewBackend(), &hungryeditor::PreviewBackend::ready);
    window.editor()->setText(doc);
    QVERIFY(ready.wait(20000)); // preview shell up with the rendered body

    const auto previewScrollY = [&] {
        int y = -1;
        window.previewBackend()->runJavaScript(QStringLiteral("Math.round(window.scrollY)"),
                                               [&](const QVariant& v) { y = v.toInt(); });
        QElapsedTimer clock;
        clock.start();
        while (y < 0 && clock.elapsed() < 5000) {
            QTest::qWait(20);
        }
        return y;
    };

    // Editor scroll drives the preview.
    window.editor()->setFirstVisibleLine(60);
    QTRY_VERIFY_WITH_TIMEOUT(previewScrollY() > 0, 10000);

    // Preview scroll drives the editor. Let the brief post-sync mute window in
    // the page expire first, otherwise the manual scroll is treated as an echo.
    window.editor()->setFirstVisibleLine(0);
    QTest::qWait(400);
    window.previewBackend()->runJavaScript(
        QStringLiteral("window.scrollTo(0, document.body.scrollHeight); void 0"));
    QTRY_VERIFY_WITH_TIMEOUT(window.editor()->firstVisibleLine() > 0, 10000);
}

void TestMainWindow::clickingAPreviewHeadingMovesTheCaret()
{
    hungryeditor::MainWindow window;
    window.resize(720, 320);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    //                              line 0        1  2      3  4       5  6
    const QString doc = QStringLiteral("# One\n\npara\n\n## Two\n\nmore\n");
    QSignalSpy ready(window.previewBackend(), &hungryeditor::PreviewBackend::ready);
    window.editor()->setText(doc);
    QVERIFY(ready.wait(20000));

    window.editor()->setCursorPosition(0, 0);
    window.previewBackend()->runJavaScript(
        QStringLiteral("document.querySelectorAll('h2')[0].click(); void 0"));

    QTRY_COMPARE_WITH_TIMEOUT(window.editor()->cursorLine(), 4, 10000);
}

void TestMainWindow::clickingAPreviewCheckboxRewritesTheSource()
{
    hungryeditor::MainWindow window;
    window.resize(720, 320);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QSignalSpy ready(window.previewBackend(), &hungryeditor::PreviewBackend::ready);
    window.editor()->setText(QStringLiteral("# Chores\n\n- [ ] water plants\n"));
    QVERIFY(ready.wait(20000));

    window.previewBackend()->runJavaScript(
        QStringLiteral("document.querySelector('input.task-checkbox').click(); void 0"));

    QTRY_COMPARE_WITH_TIMEOUT(window.editor()->text(),
                              QStringLiteral("# Chores\n\n- [x] water plants\n"), 10000);
}

void TestMainWindow::selectNextActionAddsACaret()
{
    hungryeditor::MainWindow window;
    window.editor()->setText(QStringLiteral("alpha beta alpha gamma\n"));
    window.editor()->setCursorPosition(0, 2); // inside the first "alpha"

    QAction* selectNext = window.findChild<QAction*>(QStringLiteral("action.selectNext"));
    QVERIFY(selectNext != nullptr);

    selectNext->trigger();
    QCOMPARE(window.editor()->selectionCount(), 1);
    selectNext->trigger();
    QCOMPARE(window.editor()->selectionCount(), 2);
}

void TestMainWindow::lineActionsEditTheBuffer()
{
    hungryeditor::MainWindow window;
    window.editor()->setText(QStringLiteral("keep me\n"));
    window.editor()->setCursorPosition(0, 0);

    window.findChild<QAction*>(QStringLiteral("action.duplicateLine"))->trigger();
    QCOMPARE(window.editor()->text(), QStringLiteral("keep me\nkeep me\n"));

    window.editor()->setCursorPosition(0, 0);
    window.findChild<QAction*>(QStringLiteral("action.toggleComment"))->trigger();
    QCOMPARE(window.editor()->text(), QStringLiteral("<!-- keep me -->\nkeep me\n"));
}

void TestMainWindow::formatActionsEditTheBuffer()
{
    hungryeditor::MainWindow window;
    window.editor()->setText(QStringLiteral("word\n"));
    window.editor()->setCursorPosition(0, 2);

    window.findChild<QAction*>(QStringLiteral("action.bold"))->trigger();
    QCOMPARE(window.editor()->text(), QStringLiteral("**word**\n"));

    window.editor()->setCursorPosition(0, 0);
    window.findChild<QAction*>(QStringLiteral("action.heading2"))->trigger();
    QCOMPARE(window.editor()->text(), QStringLiteral("## **word**\n"));
}

void TestMainWindow::findBarSearchesAndReplaces()
{
    hungryeditor::MainWindow window;
    window.editor()->setText(QStringLiteral("alpha beta alpha gamma alpha\n"));
    window.editor()->setCursorPosition(0, 0);

    window.findChild<QAction*>(QStringLiteral("action.find"))->trigger();
    auto* bar = window.findChild<hungryeditor::FindReplaceBar*>();
    QVERIFY(bar != nullptr);

    const auto click = [bar](const QString& text) {
        for (QToolButton* button : bar->findChildren<QToolButton*>()) {
            if (button->text() == text) {
                button->click();
                return;
            }
        }
        QFAIL("find-bar button not found");
    };

    bar->setQuery(QStringLiteral("alpha"));
    click(QStringLiteral("▼")); // next
    QCOMPARE(window.editor()->selectedText(), QStringLiteral("alpha"));

    window.findChild<QAction*>(QStringLiteral("action.replace"))->trigger();
    bar->setReplacement(QStringLiteral("A"));
    click(QStringLiteral("All"));
    QCOMPARE(window.editor()->text(), QStringLiteral("A beta A gamma A\n"));

    // Esc dismisses the bar.
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(bar, &esc);
    QVERIFY(bar->isHidden());
}

void TestMainWindow::activatingASearchResultOpensTheFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file =
        writeText(dir.filePath(QStringLiteral("notes.md")), "line one\nfind me here\nline three\n");

    hungryeditor::MainWindow window;
    auto* panel = window.searchResultsPanel();
    QVERIFY(panel != nullptr);

    const auto hits = hungryeditor::searchDirectory(dir.path(), QStringLiteral("find me"),
                                                    hungryeditor::FileSearchOptions{});
    panel->showResults(QStringLiteral("find me"), hits);
    QCOMPARE(panel->hits().size(), 1);

    panel->activateResult(0);
    QCOMPARE(window.currentPath(), file);
    QCOMPARE(window.editor()->cursorLine(), 1);
}

void TestMainWindow::outlinePanelListsHeadingsAndJumpsToThem()
{
    hungryeditor::MainWindow window;
    //                                   line 0     1  2       3  4         5  6
    window.editor()->setText(QStringLiteral("# Alpha\n\nsome text\n\n## Beta\n\nmore\n"));

    auto* panel = window.findChild<hungryeditor::OutlinePanel*>();
    QVERIFY(panel != nullptr);
    auto* tree = panel->findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);

    // The rebuild is debounced off textChanged.
    QTRY_COMPARE(tree->topLevelItemCount(), 1);
    QTreeWidgetItem* alpha = tree->topLevelItem(0);
    QCOMPARE(alpha->text(0), QStringLiteral("Alpha"));
    QCOMPARE(alpha->childCount(), 1);
    QCOMPARE(alpha->child(0)->text(0), QStringLiteral("Beta"));

    QMetaObject::invokeMethod(tree, "itemClicked", Q_ARG(QTreeWidgetItem*, alpha->child(0)),
                              Q_ARG(int, 0));
    QCOMPARE(window.editor()->cursorLine(), 4);

    QAction* toggle = window.findChild<QAction*>(QStringLiteral("action.toggleOutline"));
    QVERIFY(toggle != nullptr);
    QVERIFY(toggle->isCheckable());
}

void TestMainWindow::fileSidebarListsTheFolderAndOpensAFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeText(dir.filePath(QStringLiteral("one.md")), "one\n");
    const QString two = writeText(dir.filePath(QStringLiteral("two.md")), "two\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(dir.filePath(QStringLiteral("one.md")))); // roots the workspace here

    window.findChild<QAction*>(QStringLiteral("action.toggleFiles"))->trigger();

    auto* panel = window.findChild<hungryeditor::FileTreePanel*>();
    QVERIFY(panel != nullptr);
    auto* tree = panel->findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);

    QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItemCount() >= 2, 5000); // background scan landed

    QTreeWidgetItem* twoItem = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        if (tree->topLevelItem(i)->text(0) == QStringLiteral("two.md")) {
            twoItem = tree->topLevelItem(i);
        }
    }
    QVERIFY(twoItem != nullptr);

    QMetaObject::invokeMethod(tree, "itemActivated", Q_ARG(QTreeWidgetItem*, twoItem),
                              Q_ARG(int, 0));
    QCOMPARE(window.currentPath(), two);
}

void TestMainWindow::openFolderRootsTheSidebarAndRevealsIt()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    writeText(folder.filePath(QStringLiteral("note.md")), "n\n");
    writeText(folder.filePath(QStringLiteral("other.md")), "o\n");

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    window.openFolder(folder.path());

    QCOMPARE(window.workspaceFolder(), QDir(folder.path()).absolutePath());
    QVERIFY(window.findChild<QAction*>(QStringLiteral("action.toggleFiles"))->isChecked());
    QVERIFY(window.findChild<QAction*>(QStringLiteral("action.closeFolder"))->isEnabled());

    auto* panel = window.findChild<hungryeditor::FileTreePanel*>();
    QVERIFY(panel != nullptr);
    QCOMPARE(panel->root(), QDir(folder.path()).absolutePath());
    auto* tree = panel->findChild<QTreeWidget*>();
    QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItemCount() >= 2, 5000);

    window.openFolder(QString()); // Close Folder
    QVERIFY(window.workspaceFolder().isEmpty());
    QVERIFY(!window.findChild<QAction*>(QStringLiteral("action.closeFolder"))->isEnabled());
}

void TestMainWindow::workspaceAndFilterSurviveASessionReload()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    writeText(folder.filePath(QStringLiteral("alpha.md")), "a\n");
    writeText(folder.filePath(QStringLiteral("beta.md")), "b\n");

    {
        hungryeditor::MainWindow first;
        first.setStateDirectory(state.path());
        first.openFolder(folder.path());
        auto* panel = first.findChild<hungryeditor::FileTreePanel*>();
        auto* tree = panel->findChild<QTreeWidget*>();
        QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItemCount() >= 2, 5000);
        panel->findChild<QLineEdit*>()->setText(QStringLiteral("beta"));
        first.saveSession();
    }

    hungryeditor::MainWindow second;
    second.setStateDirectory(state.path());
    second.restoreLastSession(/*askFirst=*/false);

    QCOMPARE(second.workspaceFolder(), QDir(folder.path()).absolutePath());
    QVERIFY(second.findChild<QAction*>(QStringLiteral("action.toggleFiles"))->isChecked());
    auto* panel = second.findChild<hungryeditor::FileTreePanel*>();
    QCOMPARE(panel->findChild<QLineEdit*>()->text(), QStringLiteral("beta"));
}

namespace {

bool treeShows(QTreeWidget* tree, const QString& name)
{
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->text(0) == name) {
            return true;
        }
    }
    return false;
}

} // namespace

void TestMainWindow::sidebarCreatesRenamesAndDeletes()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    writeText(folder.filePath(QStringLiteral("seed.md")), "s\n");
    const QDir root(folder.path());

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    window.openFolder(folder.path());

    auto* tree = window.findChild<hungryeditor::FileTreePanel*>()->findChild<QTreeWidget*>();
    QTRY_VERIFY_WITH_TIMEOUT(treeShows(tree, QStringLiteral("seed.md")), 5000);

    QVERIFY(window.createFileInWorkspace(folder.path(), QStringLiteral("fresh.md")));
    QCOMPARE(window.currentPath(), root.absoluteFilePath(QStringLiteral("fresh.md")));
    QTRY_VERIFY_WITH_TIMEOUT(treeShows(tree, QStringLiteral("fresh.md")), 5000);

    QVERIFY(window.renameInWorkspace(root.absoluteFilePath(QStringLiteral("fresh.md")),
                                     QStringLiteral("renamed.md")));
    QCOMPARE(window.currentPath(), root.absoluteFilePath(QStringLiteral("renamed.md")));
    QTRY_VERIFY_WITH_TIMEOUT(treeShows(tree, QStringLiteral("renamed.md")), 5000);
    QVERIFY(!treeShows(tree, QStringLiteral("fresh.md")));

    QVERIFY(window.deleteFromWorkspace(root.absoluteFilePath(QStringLiteral("renamed.md"))));
    QVERIFY(window.currentPath() != root.absoluteFilePath(QStringLiteral("renamed.md")));
    QTRY_VERIFY_WITH_TIMEOUT(!treeShows(tree, QStringLiteral("renamed.md")), 5000);
}

void TestMainWindow::commandPaletteRunsTheChosenAction()
{
    hungryeditor::MainWindow window;
    window.editor()->setText(QStringLiteral("word word word\n"));
    window.editor()->setCursorPosition(0, 0);

    window.findChild<QAction*>(QStringLiteral("action.commandPalette"))->trigger();
    auto* palette = window.findChild<hungryeditor::CommandPalette*>();
    QVERIFY(palette != nullptr);

    auto* query = palette->findChild<QLineEdit*>();
    QVERIFY(query != nullptr);
    query->setText(QStringLiteral("select next")); // -> "Select Next Occurrence"

    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(query, &enter);

    QCOMPARE(window.editor()->selectionCount(), 1); // selectNextOccurrence ran
    QVERIFY(palette->isHidden());
}

void TestMainWindow::quickOpenOpensAFuzzilyMatchedFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeText(dir.filePath(QStringLiteral("alpha.md")), "A\n");
    const QString beta = writeText(dir.filePath(QStringLiteral("beta-notes.md")), "B\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openPath(dir.filePath(QStringLiteral("alpha.md")))); // roots the index here

    window.findChild<QAction*>(QStringLiteral("action.quickOpen"))->trigger();
    auto* palette = window.findChild<hungryeditor::CommandPalette*>();
    QVERIFY(palette != nullptr);
    auto* query = palette->findChild<QLineEdit*>();
    auto* list = palette->findChild<QListWidget*>();
    QVERIFY(query != nullptr);
    QVERIFY(list != nullptr);

    QTRY_VERIFY_WITH_TIMEOUT(list->count() >= 2, 5000); // background index landed
    query->setText(QStringLiteral("btnt"));             // fuzzy -> beta-notes
    QTRY_COMPARE(list->count(), 1);

    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(query, &enter);
    QCOMPARE(window.currentPath(), beta);
}

void TestMainWindow::mruOrderFollowsActivation()
{
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString a = writeText(files.filePath(QStringLiteral("a.md")), "a\n");
    const QString b = writeText(files.filePath(QStringLiteral("b.md")), "b\n");
    const QString c = writeText(files.filePath(QStringLiteral("c.md")), "c\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openFiles({a, b, c}));

    window.documents()->setCurrentIndex(1); // b
    window.documents()->setCurrentIndex(2); // c
    window.documents()->setCurrentIndex(0); // a

    QCOMPARE(window.mruDocumentNames(),
             (QStringList{QStringLiteral("a.md"), QStringLiteral("c.md"), QStringLiteral("b.md")}));
}

void TestMainWindow::quickSwitchWalksMruAndCommits()
{
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString a = writeText(files.filePath(QStringLiteral("a.md")), "a\n");
    const QString b = writeText(files.filePath(QStringLiteral("b.md")), "b\n");
    const QString c = writeText(files.filePath(QStringLiteral("c.md")), "c\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openFiles({a, b, c}));
    window.documents()->setCurrentIndex(1); // b
    window.documents()->setCurrentIndex(2); // c   -> mru [c, b, a]

    QAction* nextTab = window.findChild<QAction*>(QStringLiteral("action.nextTab"));
    QVERIFY(nextTab != nullptr);

    nextTab->trigger();
    auto* switcher = window.findChild<hungryeditor::TabSwitcher*>();
    QVERIFY(switcher != nullptr);
    QVERIFY(switcher->isActive());
    QCOMPARE(switcher->currentRow(), 1); // previous document (b)

    nextTab->trigger(); // steps to the third-most-recent (a)
    QCOMPARE(switcher->currentRow(), 2);

    switcher->commit();
    QVERIFY(!switcher->isActive());
    QCOMPARE(window.currentPath(), a);
}

void TestMainWindow::goToAnythingListsOpenBuffersFirst()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    writeText(folder.filePath(QStringLiteral("alpha.md")), "a\n");
    const QString beta = writeText(folder.filePath(QStringLiteral("beta.md")), "b\n");
    writeText(folder.filePath(QStringLiteral("gamma.md")), "g\n");

    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    window.openFolder(folder.path());
    QVERIFY(window.openPath(beta)); // one open buffer

    window.findChild<QAction*>(QStringLiteral("action.quickOpen"))->trigger();
    auto* palette = window.findChild<hungryeditor::CommandPalette*>();
    QVERIFY(palette != nullptr);
    auto* list = palette->findChild<QListWidget*>();
    QVERIFY(list != nullptr);

    QTRY_VERIFY_WITH_TIMEOUT(list->count() >= 3, 5000); // background index landed

    const auto nameAt = [list](int row) {
        return list->item(row)->text().split(QLatin1Char('\t')).first();
    };
    QCOMPARE(nameAt(0), QStringLiteral("beta.md")); // the open buffer leads

    int betaRows = 0;
    for (int i = 0; i < list->count(); ++i) {
        if (nameAt(i) == QStringLiteral("beta.md")) {
            ++betaRows;
        }
    }
    QCOMPARE(betaRows, 1); // listed once, not also from the file index
}

void TestMainWindow::goToLineMovesTheCaretWithinTheBuffer()
{
    hungryeditor::MainWindow window;
    window.editor()->setText(QStringLiteral("l0\nl1\nl2\nl3\nl4\nl5\n"));

    window.goToLine(4);
    QCOMPARE(window.editor()->cursorLine(), 3);

    window.goToLine(9999); // clamps to the last line
    QCOMPARE(window.editor()->cursorLine(), window.editor()->lineCount() - 1);

    window.goToLine(0); // clamps up to the first line
    QCOMPARE(window.editor()->cursorLine(), 0);
}

void TestMainWindow::caretHistoryReturnsToPriorSpots()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString a = writeText(dir.filePath(QStringLiteral("a.md")),
                                "# A0\n\np\n\n## A1\n\np\n\n### A2\n\np\n\np\n\np\n");
    const QString b = writeText(dir.filePath(QStringLiteral("b.md")), "# B\n\nbbb\n");

    hungryeditor::MainWindow window;
    QVERIFY(window.openFiles({a, b}));
    window.documents()->setCurrentIndex(0);
    window.editor()->setCursorPosition(0, 0);

    QAction* back = window.findChild<QAction*>(QStringLiteral("action.navigateBack"));
    QAction* forward = window.findChild<QAction*>(QStringLiteral("action.navigateForward"));
    QVERIFY(back != nullptr);
    QVERIFY(forward != nullptr);

    // A same-file jump is retraceable.
    window.goToLine(9);
    QVERIFY(window.editor()->cursorLine() >= 6);
    back->trigger();
    QCOMPARE(window.editor()->cursorLine(), 0);
    forward->trigger();
    QVERIFY(window.editor()->cursorLine() >= 6);

    // A cross-file jump (through the search panel) is too.
    window.editor()->setCursorPosition(2, 0);
    const auto hits = hungryeditor::searchDirectory(dir.path(), QStringLiteral("bbb"),
                                                    hungryeditor::FileSearchOptions{});
    window.searchResultsPanel()->showResults(QStringLiteral("bbb"), hits);
    window.searchResultsPanel()->activateResult(0);
    QCOMPARE(window.currentPath(), b);

    back->trigger();
    QCOMPARE(window.currentPath(), a);
    QCOMPARE(window.editor()->cursorLine(), 2);
}

void TestMainWindow::panelLayoutSurvivesAReload()
{
    QTemporaryDir state;
    QVERIFY(state.isValid());

    {
        hungryeditor::MainWindow first;
        first.setStateDirectory(state.path());
        first.resize(800, 600);
        first.show();
        QVERIFY(QTest::qWaitForWindowExposed(&first));

        first.findChild<QAction*>(QStringLiteral("action.toggleOutline"))->trigger(); // reveal it
        auto* splitter = first.findChild<QSplitter*>();
        QVERIFY(splitter != nullptr);
        splitter->setSizes({250, 550}); // a deliberately uneven split
        first.saveSession();
    }

    hungryeditor::MainWindow second;
    second.setStateDirectory(state.path());
    second.restoreLastSession(/*askFirst=*/false);
    second.resize(800, 600);
    second.show();
    QVERIFY(QTest::qWaitForWindowExposed(&second));

    QVERIFY(second.findChild<QAction*>(QStringLiteral("action.toggleOutline"))->isChecked());

    const QList<int> sizes = second.findChild<QSplitter*>()->sizes();
    QCOMPARE(sizes.size(), 2);
    QVERIFY(sizes.at(0) > 0);
    QVERIFY(sizes.at(1) > 0);
    QVERIFY(sizes.at(0) < sizes.at(1)); // the uneven split was restored, not the default
}

QTEST_MAIN(TestMainWindow)
#include "test_main_window.moc"
