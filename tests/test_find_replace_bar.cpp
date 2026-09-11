// Keyboard-navigation and screen-reader-label coverage for the find/replace
// bar: every one of its several focusable children should be reachable by
// Tab and should still let Escape dismiss the whole bar.

#include <QApplication>
#include <QLineEdit>
#include <QSignalSpy>
#include <QToolButton>
#include <QtTest>

#include "ui/FindReplaceBar.h"

using hungryeditor::FindReplaceBar;

class TestFindReplaceBar : public QObject
{
    Q_OBJECT

private slots:
    void everyControlHasAMeaningfulAccessibleName();
    void tabVisitsEveryControlInOrder();
    void escapeDismissesFromAnyChildWidget();
};

void TestFindReplaceBar::everyControlHasAMeaningfulAccessibleName()
{
    FindReplaceBar bar;
    bar.reveal(true);

    // Every focusable child should have an accessible name distinct from
    // whatever decorative glyph its visible text might be — a screen reader
    // reads this, not the glyph.
    for (QWidget* child : bar.findChildren<QWidget*>()) {
        if (child->focusPolicy() == Qt::NoFocus) {
            continue;
        }
        QVERIFY2(!child->accessibleName().isEmpty(),
                 qPrintable(QStringLiteral("a focusable child (objectName '%1', text-bearing: %2) "
                                           "has no accessible name")
                                .arg(child->objectName())
                                .arg(child->property("text").toString())));
    }
}

void TestFindReplaceBar::tabVisitsEveryControlInOrder()
{
    FindReplaceBar bar;
    bar.show();
    bar.reveal(true);
    QVERIFY(QTest::qWaitForWindowExposed(&bar));

    const QStringList expected = {
        QStringLiteral("Find"),
        QStringLiteral("Previous match"),
        QStringLiteral("Next match"),
        QStringLiteral("Match case"),
        QStringLiteral("Whole word"),
        QStringLiteral("Regular expression"),
        QStringLiteral("Close find bar"),
        QStringLiteral("Replace"),
        QStringLiteral("Replace current match"),
        QStringLiteral("Replace all matches"),
    };

    for (const QString& name : expected) {
        QWidget* focused = QApplication::focusWidget();
        QVERIFY2(focused != nullptr,
                 qPrintable(QStringLiteral("expected focus on '%1', got none").arg(name)));
        QCOMPARE(focused->accessibleName(), name);
        QTest::keyClick(focused, Qt::Key_Tab);
    }
}

void TestFindReplaceBar::escapeDismissesFromAnyChildWidget()
{
    FindReplaceBar bar;
    bar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bar));

    for (QWidget* child : bar.findChildren<QWidget*>()) {
        if (child->focusPolicy() == Qt::NoFocus) {
            continue;
        }
        bar.reveal(true);
        child->setFocus();
        QSignalSpy dismissed(&bar, &FindReplaceBar::dismissed);
        QTest::keyClick(child, Qt::Key_Escape);
        QVERIFY2(dismissed.count() == 1,
                 qPrintable(QStringLiteral("Escape from '%1' did not dismiss the bar")
                                .arg(child->accessibleName())));
    }
}

QTEST_MAIN(TestFindReplaceBar)
#include "test_find_replace_bar.moc"
