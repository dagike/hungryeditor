// Coverage for the debounced background highlight pipeline.

#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include "highlight/HighlightController.h"

extern "C" const TSLanguage* tree_sitter_markdown(void);

class TestHighlightWorker : public QObject
{
    Q_OBJECT

private slots:
    void parsesOnASeparateThread();
    void burstOfEditsProducesOneResultForTheLatestRevision();
    void resultCarriesTreeSummary();
};

void TestHighlightWorker::parsesOnASeparateThread()
{
    hungryeditor::HighlightController controller;
    QVERIFY(controller.workerThread() != QThread::currentThread());
    QVERIFY(controller.workerThread()->isRunning());
}

void TestHighlightWorker::burstOfEditsProducesOneResultForTheLatestRevision()
{
    hungryeditor::HighlightController controller;
    controller.setLanguage(tree_sitter_markdown());

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);

    quint64 last = 0;
    for (int i = 0; i < 20; ++i) {
        last = controller.submit(QStringLiteral("# heading %1\n\ntext\n").arg(i));
    }

    // One debounced parse should land, for the final revision only.
    QVERIFY(spy.wait(2000));
    QTest::qWait(50); // allow any stragglers
    QCOMPARE(spy.count(), 1);

    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QCOMPARE(result.revision, last);
    QCOMPARE(controller.lastResultRevision(), last);
}

void TestHighlightWorker::resultCarriesTreeSummary()
{
    hungryeditor::HighlightController controller;
    controller.setLanguage(tree_sitter_markdown());

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    controller.submit(QStringLiteral("# Title\n\npara\n\n```rust\nfn main() {}\n```\n"));

    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);
    QCOMPARE(result.rootType, QStringLiteral("document"));
    QVERIFY(result.namedChildCount > 0);
}

QTEST_MAIN(TestHighlightWorker)
#include "test_highlight_worker.moc"
