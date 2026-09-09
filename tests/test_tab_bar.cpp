// Coverage for the tab strip kept in step with the document model.

#include <QSignalSpy>
#include <QtTest>

#include "app/MainWindow.h"
#include "app/TabBar.h"
#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"

using hungryeditor::MainWindow;

namespace {

void addDocument(MainWindow& window)
{
    window.findChild<QAction*>(QStringLiteral("action.new"))->trigger();
}

} // namespace

class TestTabBar : public QObject
{
    Q_OBJECT

private slots:
    void tabTracksEachDocument();
    void tabStripHidesForASingleDocument();
    void dirtyMarkerTogglesWithModifiedState();
    void clickingATabSwitchesDocument();
    void closingATabRemovesTheDocument();
    void draggingATabReordersDocuments();
    void renamingOnSaveUpdatesTabText();
};

void TestTabBar::tabTracksEachDocument()
{
    MainWindow window;
    QCOMPARE(window.tabBar()->count(), 1);

    addDocument(window);
    QCOMPARE(window.tabBar()->count(), 2);
    QCOMPARE(window.tabBar()->tabText(1), QStringLiteral("Untitled 1"));
    QCOMPARE(window.tabBar()->currentIndex(), 1);
}

void TestTabBar::tabStripHidesForASingleDocument()
{
    MainWindow window;
    QVERIFY(window.tabBar()->isHidden());

    addDocument(window);
    QVERIFY(!window.tabBar()->isHidden());

    window.closeDocumentAt(1);
    QVERIFY(window.tabBar()->isHidden());
}

void TestTabBar::dirtyMarkerTogglesWithModifiedState()
{
    MainWindow window;
    addDocument(window);

    window.editor()->setText(QStringLiteral("changed"));
    QCOMPARE(window.tabBar()->tabText(1), QStringLiteral("*Untitled 1"));

    window.editor()->markClean();
    QCOMPARE(window.tabBar()->tabText(1), QStringLiteral("Untitled 1"));
}

void TestTabBar::clickingATabSwitchesDocument()
{
    MainWindow window;
    window.editor()->setText(QStringLiteral("first buffer"));
    addDocument(window);
    window.editor()->setText(QStringLiteral("second buffer"));

    window.tabBar()->setCurrentIndex(0);
    QCOMPARE(window.documents()->currentIndex(), 0);
    QCOMPARE(window.editor()->text(), QStringLiteral("first buffer"));

    window.tabBar()->setCurrentIndex(1);
    QCOMPARE(window.editor()->text(), QStringLiteral("second buffer"));
}

void TestTabBar::closingATabRemovesTheDocument()
{
    MainWindow window;
    addDocument(window);
    addDocument(window);
    QCOMPARE(window.tabBar()->count(), 3);

    emit window.tabBar()->tabCloseRequested(1);

    QCOMPARE(window.tabBar()->count(), 2);
    QCOMPARE(window.documents()->count(), 2);
}

void TestTabBar::draggingATabReordersDocuments()
{
    MainWindow window;
    window.editor()->setText(QStringLiteral("doc A"));
    addDocument(window);
    window.editor()->setText(QStringLiteral("doc B"));
    addDocument(window);
    window.editor()->setText(QStringLiteral("doc C"));
    QCOMPARE(window.documents()->currentIndex(), 2);

    hungryeditor::Document* docA = window.documents()->documentAt(0);
    hungryeditor::Document* docC = window.documents()->documentAt(2);

    window.tabBar()->moveTab(0, 2); // A jumps to the end

    QCOMPARE(window.documents()->documentAt(2), docA);
    QCOMPARE(window.documents()->current(), docC);
    QCOMPARE(window.documents()->currentIndex(), window.tabBar()->currentIndex());
    QCOMPARE(window.editor()->text(), QStringLiteral("doc C"));
}

void TestTabBar::renamingOnSaveUpdatesTabText()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MainWindow window;
    addDocument(window);
    window.editor()->setText(QStringLiteral("content\n"));
    QCOMPARE(window.tabBar()->tabText(1), QStringLiteral("*Untitled 1"));

    QVERIFY(window.savePath(dir.filePath(QStringLiteral("named.md"))));
    QCOMPARE(window.tabBar()->tabText(1), QStringLiteral("named.md"));
}

QTEST_MAIN(TestTabBar)
#include "test_tab_bar.moc"
