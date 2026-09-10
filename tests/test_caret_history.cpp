// Coverage for the back/forward caret-position history.

#include <QtTest>

#include "editor/CaretHistory.h"

using hungryeditor::CaretHistory;
using hungryeditor::CaretLocation;

namespace {

CaretLocation at(const QString& path, int line)
{
    return {path, line, 0};
}

} // namespace

class TestCaretHistory : public QObject
{
    Q_OBJECT

private slots:
    void backAndForwardWalkTheTrail();
    void recordingAfterGoingBackDropsTheForwardTail();
    void repeatedRecordsAtTheSameSpotCoalesce();
    void theTrailIsBounded();
};

void TestCaretHistory::backAndForwardWalkTheTrail()
{
    CaretHistory history;
    QVERIFY(!history.canGoBack());
    QVERIFY(!history.canGoForward());

    history.record(at(QStringLiteral("a.md"), 0));
    history.record(at(QStringLiteral("a.md"), 40));

    QVERIFY(history.canGoBack());
    const CaretLocation first = history.goBack(at(QStringLiteral("a.md"), 80));
    QCOMPARE(first.line, 40);
    QVERIFY(history.canGoForward());

    const CaretLocation second = history.goBack(at(QStringLiteral("a.md"), 40));
    QCOMPARE(second.line, 0);
    QVERIFY(!history.canGoBack());

    const CaretLocation forward = history.goForward(at(QStringLiteral("a.md"), 0));
    QCOMPARE(forward.line, 40);
}

void TestCaretHistory::recordingAfterGoingBackDropsTheForwardTail()
{
    CaretHistory history;
    history.record(at(QStringLiteral("a.md"), 0));
    history.record(at(QStringLiteral("a.md"), 50));

    history.goBack(at(QStringLiteral("a.md"), 90)); // forward now holds line 90
    QVERIFY(history.canGoForward());

    history.record(at(QStringLiteral("a.md"), 200)); // a new branch
    QVERIFY(!history.canGoForward());
    QVERIFY(history.canGoBack());
}

void TestCaretHistory::repeatedRecordsAtTheSameSpotCoalesce()
{
    CaretHistory history;
    history.record({QStringLiteral("a.md"), 10, 0});
    history.record({QStringLiteral("a.md"), 10, 4}); // same line
    history.record({QStringLiteral("a.md"), 11, 0}); // within kMergeLines
    QCOMPARE(history.size(), 1);

    history.record(at(QStringLiteral("a.md"), 40)); // clearly elsewhere
    QCOMPARE(history.size(), 2);

    history.record(at(QStringLiteral("b.md"), 11)); // other file, never coalesces
    QCOMPARE(history.size(), 3);
}

void TestCaretHistory::theTrailIsBounded()
{
    CaretHistory history;
    for (int i = 0; i < CaretHistory::kMax + 20; ++i) {
        history.record(at(QStringLiteral("a.md"), i * 5));
    }
    QCOMPARE(history.size(), CaretHistory::kMax);
}

QTEST_APPLESS_MAIN(TestCaretHistory)
#include "test_caret_history.moc"
