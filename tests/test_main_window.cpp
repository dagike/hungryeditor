// Smoke coverage for the application window.

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QMenuBar>
#include <QMimeData>
#include <QTemporaryDir>
#include <QtTest>
#include <QUrl>

#include "app/MainWindow.h"
#include "app/TabBar.h"
#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"
#include "io/DraftStore.h"

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
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
    void cleanShutdownClearsDrafts();
    void newAndSwitchActionsChangeCurrentDocument();
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
    QCOMPARE(window.menuBar()->actions().size(), 2);
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

void TestMainWindow::cleanShutdownClearsDrafts()
{
    QTemporaryDir drafts;
    QVERIFY(drafts.isValid());

    hungryeditor::MainWindow window;
    window.documents()->setDraftDirectory(drafts.path());
    window.editor()->setText(QStringLiteral("unsaved edits\n"));
    window.documents()->autosaveDirtyDocuments();
    QVERIFY(!hungryeditor::DraftStore(drafts.path()).loadAll().isEmpty());

    window.documents()->clearDrafts(); // what the aboutToQuit hook runs
    QVERIFY(hungryeditor::DraftStore(drafts.path()).loadAll().isEmpty());
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

QTEST_MAIN(TestMainWindow)
#include "test_main_window.moc"
