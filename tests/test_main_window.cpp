// Smoke coverage for the application window.

#include <QAction>
#include <QMenuBar>
#include <QtTest>

#include <ScintillaEditBase.h>

#include "app/MainWindow.h"

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void editorIsCentralWidget();
    void hasExpectedMenus();
    void hasNamedActions_data();
    void hasNamedActions();
    void showsWithoutCrashing();
};

void TestMainWindow::editorIsCentralWidget()
{
    hungryeditor::MainWindow window;
    QVERIFY(window.editor() != nullptr);
    QCOMPARE(window.centralWidget(), static_cast<QWidget*>(window.editor()));
}

void TestMainWindow::hasExpectedMenus()
{
    hungryeditor::MainWindow window;
    QCOMPARE(window.menuBar()->actions().size(), 2);
}

void TestMainWindow::hasNamedActions_data()
{
    QTest::addColumn<QString>("objectName");
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

QTEST_MAIN(TestMainWindow)
#include "test_main_window.moc"
