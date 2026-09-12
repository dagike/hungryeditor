// Coverage for the debounced background highlight pipeline.

#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include "highlight/CaptureStyles.h"
#include "highlight/HighlightController.h"
#include "highlight/TreeSitterEngine.h"

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

/// A PendingEdit appending `after.substr(before.size())` to `before`, for a
/// submission whose text is `after`.
hungryeditor::PendingEdit appendEdit(const QString& before, const QString& after)
{
    const std::string beforeUtf8 = before.toStdString();
    const std::string afterUtf8 = after.toStdString();
    const auto start = static_cast<uint32_t>(beforeUtf8.size());
    const auto newEnd = static_cast<uint32_t>(afterUtf8.size());
    using hungryeditor::TreeSitterEngine;
    const TSInputEdit edit{
        start,
        start,
        newEnd,
        TreeSitterEngine::pointAt(beforeUtf8, start),
        TreeSitterEngine::pointAt(beforeUtf8, start),
        TreeSitterEngine::pointAt(afterUtf8, newEnd),
    };
    return hungryeditor::PendingEdit{/*present=*/true, edit};
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
    void disabledControllerIgnoresSubmissions();
    void incrementalSubmitMatchesFullReparse();
    void multipleQueuedEditsProduceOneCorrectResult();
    void boundedRangeYieldsSpansCoveringJustThatRange();
    void unchangedTextSkipsReparseAndReusesTheExistingTree();
    void rangeExtensionCoalescedWithAnEditStaysCorrect();
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

void TestHighlightWorker::disabledControllerIgnoresSubmissions()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    controller.setEnabled(false);
    QVERIFY(!controller.isEnabled());
    controller.submit(QStringLiteral("# ignored\n"));

    QVERIFY(!spy.wait(300)); // nothing is parsed while disabled

    controller.setEnabled(true);
    controller.submit(QStringLiteral("# picked up\n"));
    QVERIFY(spy.wait(2000));
}

void TestHighlightWorker::incrementalSubmitMatchesFullReparse()
{
    hungryeditor::HighlightController incremental;
    incremental.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    const QString doc0 = QStringLiteral("# Heading\n\ntext\n");
    const QString doc1 = doc0 + QStringLiteral("\n```rust\nfn demo() -> i32 { 0 }\n```\n");

    QSignalSpy spy(&incremental, &hungryeditor::HighlightController::highlighted);
    incremental.submit(doc0); // no edit: establishes the tree doc1's edit applies against
    QVERIFY(spy.wait(2000));
    spy.clear();

    incremental.submit(doc1, appendEdit(doc0, doc1));
    QVERIFY(spy.wait(2000));
    const auto incrementalResult = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(incrementalResult.ok);

    hungryeditor::HighlightController full;
    full.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);
    QSignalSpy fullSpy(&full, &hungryeditor::HighlightController::highlighted);
    full.submit(doc1);
    QVERIFY(fullSpy.wait(2000));
    const auto fullResult = fullSpy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(fullResult.ok);

    // The incremental path (noteEdit + reparse) must produce byte-identical
    // output to a plain full reparse of the same final text.
    QCOMPARE(incrementalResult.rootType, fullResult.rootType);
    QCOMPARE(incrementalResult.namedChildCount, fullResult.namedChildCount);
    QCOMPARE(incrementalResult.spans.size(), fullResult.spans.size());
    for (int i = 0; i < incrementalResult.spans.size(); ++i) {
        QCOMPARE(incrementalResult.spans[i].start, fullResult.spans[i].start);
        QCOMPARE(incrementalResult.spans[i].length, fullResult.spans[i].length);
        QCOMPARE(incrementalResult.spans[i].style, fullResult.spans[i].style);
    }
}

void TestHighlightWorker::multipleQueuedEditsProduceOneCorrectResult()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    // Establish a real tree first, and let that settle — a burst that
    // includes the very first (tree-less) submission would always take the
    // full-reparse fallback regardless, which isn't what this test means to
    // exercise.
    QString text = QStringLiteral("intro\n");
    QSignalSpy setupSpy(&controller, &hungryeditor::HighlightController::highlighted);
    controller.submit(text);
    QVERIFY(setupSpy.wait(2000));

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);

    // Three edits fired back-to-back, well inside one debounce window — the
    // shape a fast typist, or a delete+insert replace-selection, produces.
    // All three must fold into one incremental reparse.
    const QString step1 = text + QStringLiteral("# Heading\n");
    controller.submit(step1, appendEdit(text, step1));
    text = step1;

    const QString step2 = text + QStringLiteral("\ntext\n");
    controller.submit(step2, appendEdit(text, step2));
    text = step2;

    const QString step3 = text + QStringLiteral("\n```rust\nfn demo() {}\n```\n");
    controller.submit(step3, appendEdit(text, step3));
    text = step3;

    QVERIFY(spy.wait(2000));
    QTest::qWait(50); // allow any stragglers
    QCOMPARE(spy.count(), 1);

    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);

    hungryeditor::HighlightController full;
    full.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);
    QSignalSpy fullSpy(&full, &hungryeditor::HighlightController::highlighted);
    full.submit(text);
    QVERIFY(fullSpy.wait(2000));
    const auto fullResult = fullSpy.first().at(0).value<hungryeditor::HighlightResult>();

    QCOMPARE(result.spans.size(), fullResult.spans.size());
    for (int i = 0; i < result.spans.size(); ++i) {
        QCOMPARE(result.spans[i].start, fullResult.spans[i].start);
        QCOMPARE(result.spans[i].length, fullResult.spans[i].length);
        QCOMPARE(result.spans[i].style, fullResult.spans[i].style);
    }
}

void TestHighlightWorker::boundedRangeYieldsSpansCoveringJustThatRange()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    const QString doc = QStringLiteral("# Heading\n\n```rust\nfn demo() {}\n```\n\nmore text\n");
    controller.submit(doc);
    QVERIFY(spy.wait(2000));
    spy.clear();

    // A bounded range covering only the heading and the fence, not the rest
    // of the document.
    const QByteArray bytes = doc.toUtf8();
    const auto rangeStart = static_cast<quint32>(0);
    const auto rangeEnd = static_cast<quint32>(bytes.indexOf("more text"));
    controller.submit(doc, {}, hungryeditor::HighlightRange{rangeStart, rangeEnd});
    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);

    QCOMPARE(result.rangeStart, rangeStart);
    QCOMPARE(result.rangeEnd, rangeEnd);
    QVERIFY(!result.spans.isEmpty());
    quint32 covered = rangeStart;
    for (const auto& span : result.spans) {
        QCOMPARE(span.start, covered); // contiguous, starting at rangeStart — not byte 0
        covered += span.length;
    }
    QCOMPARE(covered, rangeEnd); // and stopping exactly at rangeEnd
}

void TestHighlightWorker::unchangedTextSkipsReparseAndReusesTheExistingTree()
{
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    const QString doc = QStringLiteral("# Real Heading\n\ntext\n");
    controller.submit(doc);
    QVERIFY(spy.wait(2000));
    spy.clear();

    // Asserts nothing changed, but with text that (if actually reparsed)
    // would produce completely different spans — proving the worker really
    // does skip reparsing and trusts the existing tree, not this
    // submission's own (deliberately wrong) text.
    const QString wrongText = QStringLiteral("totally different content, no heading at all");
    controller.submit(wrongText, {}, hungryeditor::HighlightRange{}, /*textChanged=*/false);
    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);
    QCOMPARE(result.rootType, QStringLiteral("document")); // the ORIGINAL tree, untouched

    const QByteArray bytes = doc.toUtf8(); // spans must key against the ORIGINAL doc's offsets
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("Real Heading"))),
             static_cast<qint32>(hungryeditor::StyleHeading));
}

void TestHighlightWorker::rangeExtensionCoalescedWithAnEditStaysCorrect()
{
    // A real edit immediately followed, within the same debounce window, by
    // a pure range-extension request — the shape Editor::onNotify() produces
    // when an edit and a Notification::StyleNeeded scroll-refresh land close
    // together. The range-extension call must not cause the edit to be
    // dropped from the batch (a real bug this once was, in
    // HighlightWorker::submit()'s pendingEditsAllPresent_ bookkeeping).
    hungryeditor::HighlightController controller;
    controller.configure(tree_sitter_markdown(), kMarkdownQuery, kMarkdownInjections);

    QSignalSpy spy(&controller, &hungryeditor::HighlightController::highlighted);
    const QString doc0 = QStringLiteral("# Heading\n\ntext\n");
    controller.submit(doc0);
    QVERIFY(spy.wait(2000));
    spy.clear();

    const QString doc1 = doc0 + QStringLiteral("\n```rust\nfn demo() {}\n```\n");
    controller.submit(doc1, appendEdit(doc0, doc1));
    controller.submit(doc1, {},
                      hungryeditor::HighlightRange{0, static_cast<quint32>(doc1.toUtf8().size())},
                      /*textChanged=*/false);
    QVERIFY(spy.wait(2000));
    const auto result = spy.first().at(0).value<hungryeditor::HighlightResult>();
    QVERIFY(result.ok);

    const QByteArray bytes = doc1.toUtf8();
    QCOMPARE(styleAtByte(result, static_cast<int>(bytes.indexOf("fn "))),
             static_cast<qint32>(hungryeditor::StyleKeyword));
}

QTEST_MAIN(TestHighlightWorker)
#include "test_highlight_worker.moc"
