// Smoke coverage for the application window.

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
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
#include "ui/FindReplaceBar.h"
#include "ui/SearchResultsPanel.h"
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
    void selectNextActionAddsACaret();
    void findBarSearchesAndReplaces();
    void activatingASearchResultOpensTheFile();
    void commandPaletteRunsTheChosenAction();
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
    QCOMPARE(window.menuBar()->actions().size(), 4); // File, Edit, View, Help
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
    QTest::newRow("commandPalette") << QStringLiteral("action.commandPalette");
    QTest::newRow("selectNext") << QStringLiteral("action.selectNext");
    QTest::newRow("viewEditor") << QStringLiteral("action.viewEditor");
    QTest::newRow("viewSplit") << QStringLiteral("action.viewSplit");
    QTest::newRow("viewPreview") << QStringLiteral("action.viewPreview");
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

QTEST_MAIN(TestMainWindow)
#include "test_main_window.moc"
