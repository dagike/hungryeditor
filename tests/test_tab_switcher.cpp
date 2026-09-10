// Coverage for the Ctrl+Tab document-switching overlay.

#include <QListWidget>
#include <QSignalSpy>
#include <QtTest>

#include "ui/TabSwitcher.h"

using hungryeditor::TabSwitcher;

namespace {

QList<TabSwitcher::Entry> sample()
{
    return {
        {QStringLiteral("alpha.md"), QStringLiteral("/ws"), false},
        {QStringLiteral("beta.md"), QStringLiteral("/ws/docs"), true},
        {QStringLiteral("gamma.md"), QStringLiteral("/ws"), false},
    };
}

} // namespace

class TestTabSwitcher : public QObject
{
    Q_OBJECT

private slots:
    void presentHighlightsTheStartRow();
    void ignoresAnEmptyList();
    void steppingWrapsBothWays();
    void commitEmitsTheHighlightedRowOnce();
    void cancelEmitsNothingChosen();
};

void TestTabSwitcher::presentHighlightsTheStartRow()
{
    TabSwitcher switcher;
    switcher.present(sample(), 1);

    QVERIFY(switcher.isActive());
    QCOMPARE(switcher.findChild<QListWidget*>()->count(), 3);
    QCOMPARE(switcher.currentRow(), 1);

    // A start row past the end clamps to the last entry.
    switcher.cancel();
    switcher.present(sample(), 99);
    QCOMPARE(switcher.currentRow(), 2);
}

void TestTabSwitcher::ignoresAnEmptyList()
{
    TabSwitcher switcher;
    switcher.present({}, 0);
    QVERIFY(!switcher.isActive());
}

void TestTabSwitcher::steppingWrapsBothWays()
{
    TabSwitcher switcher;
    switcher.present(sample(), 2);

    switcher.selectNext(); // 2 -> 0
    QCOMPARE(switcher.currentRow(), 0);
    switcher.selectPrevious(); // 0 -> 2
    QCOMPARE(switcher.currentRow(), 2);
    switcher.selectPrevious(); // 2 -> 1
    QCOMPARE(switcher.currentRow(), 1);
}

void TestTabSwitcher::commitEmitsTheHighlightedRowOnce()
{
    TabSwitcher switcher;
    switcher.present(sample(), 1);
    switcher.selectNext(); // -> 2

    QSignalSpy accepted(&switcher, &TabSwitcher::accepted);
    switcher.commit();

    QCOMPARE(accepted.count(), 1);
    QCOMPARE(accepted.first().first().toInt(), 2);
    QVERIFY(!switcher.isActive());

    switcher.commit(); // already closed — no second emission
    QCOMPARE(accepted.count(), 1);
}

void TestTabSwitcher::cancelEmitsNothingChosen()
{
    TabSwitcher switcher;
    switcher.present(sample(), 0);

    QSignalSpy accepted(&switcher, &TabSwitcher::accepted);
    QSignalSpy cancelled(&switcher, &TabSwitcher::cancelled);
    switcher.cancel();

    QCOMPARE(accepted.count(), 0);
    QCOMPARE(cancelled.count(), 1);
    QVERIFY(!switcher.isActive());
}

QTEST_MAIN(TestTabSwitcher)
#include "test_tab_switcher.moc"
