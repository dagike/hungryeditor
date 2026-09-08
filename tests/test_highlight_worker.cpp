// Coverage for the debounced background highlight pipeline.

#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include "highlight/CaptureStyles.h"
#include "highlight/HighlightController.h"

extern "C" const TSLanguage* tree_sitter_markdown(void);

namespace {
// A small slice of the real markdown highlights query, enough to exercise
// span production without depending on the bundled resource file.
const QString kMarkdownQuery = QStringLiteral(R"scm(
(atx_heading (inline) @text.title)
[
  (fenced_code_block)
] @text.literal
[
  (atx_h1_marker)
  (atx_h2_marker)
] @punctuation.special
)scm");

// Injections: markdown-inline over prose, and a foreign grammar over fenced
// code (identified by its info string).
const QString kMarkdownInjections = QStringLiteral(R"scm(
(fenced_code_block
  (info_string (language) @injection.language)
  (code_fence_content) @injection.content)
((inline) @injection.content (#set! injection.language "markdown_inline"))
)scm");

qint32 styleAtByte(const hungryeditor::HighlightResult& result, int byte)
{
    for (const auto& span : result.spans) {
        if (static_cast<int>(span.start) <= byte &&
            byte < static_cast<int>(span.start + span.length)) {
            return span.style;
        }
    }
    return -1;
}
} // namespace

class TestHighlightWorker : public QObject
{
    Q_OBJECT

private slots:
    void parsesOnASeparateThread();
    void burstOfEditsProducesOneResultForTheLatestRevision();
    void resultCarriesTreeSummary();
    void spansCoverHeadingsAndCode();
    void inlineInjectionStylesEmphasisAndStrong();
    void unknownFenceLanguageKeepsLiteralStyle();
    void fencedCodeIsStyledByItsLanguageGrammar();
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
    controller.configure(tree_sitter_markdown(), kMarkdownQuery);

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
    controller.configure(tree_sitter_markdown(), kMarkdownQuery);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    controller.submit(QStringLiteral("# Title\n\npara\n\n```rust\nfn main() {}\n```\n"));

    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);
    QCOMPARE(result.rootType, QStringLiteral("document"));
    QVERIFY(result.namedChildCount > 0);
}

void TestHighlightWorker::spansCoverHeadingsAndCode()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    const QString doc = QStringLiteral("# Heading\n\nplain paragraph\n\n```c\nint x;\n```\n");
    controller.submit(doc);

    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);
    QVERIFY(!result.spans.isEmpty());

    // Spans are contiguous and cover the whole document exactly once.
    quint32 next = 0;
    quint32 headingBytes = 0;
    quint32 codeBytes = 0;
    for (const auto& span : result.spans) {
        QCOMPARE(span.start, next);
        next += span.length;
        if (span.style == hungryeditor::StyleHeading) {
            headingBytes += span.length;
        } else if (span.style == hungryeditor::StyleCodeLiteral) {
            codeBytes += span.length;
        }
    }
    QCOMPARE(next, static_cast<quint32>(doc.toUtf8().size()));
    QVERIFY(headingBytes > 0);
    QVERIFY(codeBytes > 0);

    // The word "plain" is inside a bare paragraph — it must stay unstyled.
    const int plainOffset = static_cast<int>(doc.toUtf8().indexOf("plain"));
    for (const auto& span : result.spans) {
        if (static_cast<int>(span.start) <= plainOffset &&
            plainOffset < static_cast<int>(span.start + span.length)) {
            QCOMPARE(span.style, static_cast<qint32>(hungryeditor::StylePlain));
        }
    }
}

void TestHighlightWorker::inlineInjectionStylesEmphasisAndStrong()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    const QString doc = QStringLiteral("plain *soft* and **loud** words\n");
    controller.submit(doc);

    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);

    const QByteArray bytes = doc.toUtf8();
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("soft"))),
             static_cast<qint32>(hungryeditor::StyleEmphasis));
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("loud"))),
             static_cast<qint32>(hungryeditor::StyleStrong));
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("plain"))),
             static_cast<qint32>(hungryeditor::StylePlain));
}

void TestHighlightWorker::unknownFenceLanguageKeepsLiteralStyle()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    // No grammar is registered for "nonesuch" — the block must not crash and
    // the fenced content keeps the block-level literal style.
    const QString doc = QStringLiteral("intro *em* text\n\n```nonesuch\nfn demo() {}\n```\n");
    controller.submit(doc);

    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);

    const QByteArray bytes = doc.toUtf8();
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("em"))),
             static_cast<qint32>(hungryeditor::StyleEmphasis));
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("fn demo"))),
             static_cast<qint32>(hungryeditor::StyleCodeLiteral));
}

void TestHighlightWorker::fencedCodeIsStyledByItsLanguageGrammar()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    const QString doc = QStringLiteral("```rust\nfn demo() -> i32 { 0 }\n```\n");
    controller.submit(doc);

    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);

    const QByteArray bytes = doc.toUtf8();
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("fn "))),
             static_cast<qint32>(hungryeditor::StyleKeyword));
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("i32"))),
             static_cast<qint32>(hungryeditor::StyleType));
}

QTEST_MAIN(TestHighlightWorker)
#include "test_highlight_worker.moc"
