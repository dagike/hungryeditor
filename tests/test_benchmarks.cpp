// Performance regression coverage for the success criteria in the project
// plan: cold start, edit latency, preview refresh, and (in
// HUNGRYEDITOR_STRICT_BENCHMARKS builds only) idle memory.
//
// The plan's numbers (cold start < 250 ms, keystroke-to-paint < 8 ms, preview
// refresh < 100 ms, 10 MB file open < 500 ms, idle RSS < 120 MB) are
// release-build targets on a real desktop. By default this suite runs
// headless, unoptimized (debug build) and often on a shared, noisy CI VM, so
// the QVERIFY thresholds below are deliberately far looser than those
// targets — they exist to catch a gross regression (an accidental
// order-of-magnitude blowup), not to police the product targets. The
// dedicated release-benchmarks CI job builds linux-release with
// HUNGRYEDITOR_STRICT_BENCHMARKS on instead, which is when the targets
// actually get asserted — see each test for the real, measured number where
// a target isn't currently met. The QBENCHMARK slots report real, trendable
// numbers alongside them either way.

#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

#include "app/MainWindow.h"
#include "editor/Editor.h"
#include "preview/PreviewBackend.h"
#include "preview/PreviewController.h"

using hungryeditor::Editor;
using hungryeditor::MainWindow;
using hungryeditor::PreviewBackend;
using hungryeditor::PreviewController;

namespace {

/// Records what the controller pushes, without a browser — isolates the
/// render pipeline's own cost (Markdown parse + image resolution) from
/// Chromium and the WebChannel round-trip.
class FakeBackend : public PreviewBackend
{
public:
    QWidget* widget() override { return nullptr; }
    void setHtml(const QString&, const QUrl&) override {}
    void setContent(const QString&, const QUrl&) override {}
    void setThemeCss(const QString&) override {}
    void runJavaScript(const QString&, const std::function<void(const QVariant&)>&) override {}
    void scrollToSourceLine(int) override {}
    bool print(QPrinter*) override { return false; }
    bool printToPdf(const QString&) override { return false; }
};

/// A markdown document with real structure (headings, prose, a list, a
/// handful of fenced code blocks) sized to roughly `bytes`, for a
/// representative render workload. Fences are scattered sparingly (about one
/// per 20 KB) rather than one per paragraph — a real technical document has
/// far more prose than code, and each distinct language fence recompiles a
/// tree-sitter query until that's cached (see the 9.3 fix), so a document
/// that is mostly fences would benchmark that cost, not a realistic one.
QString syntheticMarkdown(qsizetype bytes)
{
    const QString prose =
        QStringLiteral("## Section\n\n"
                       "Some *ordinary* prose with a [link](https://example.com) and `code`.\n\n"
                       "- one\n- two\n- three\n\n");
    const QString fence =
        QStringLiteral("```cpp\nint add(int a, int b) { return a + b; }\n```\n\n");

    QString text;
    text.reserve(bytes + prose.size());
    qsizetype nextFenceAt = 0;
    while (text.size() < bytes) {
        text += prose;
        if (text.size() >= nextFenceAt) {
            text += fence;
            nextFenceAt = text.size() + 20 * 1024;
        }
    }
    return text;
}

/// A large, but realistically-shaped, document: long-form prose with only
/// occasional headings — unlike syntheticMarkdown() above, this does not put
/// a heading and a list in every paragraph. That density matters here:
/// Editor::updateHighlightTier()'s Lexilla fallback (for anything over its
/// tree-sitter byte limit) runs Colourise() synchronously over the whole
/// buffer on load, and Scintilla/Lexilla's per-line bookkeeping scales with
/// block-boundary *count*, not just byte size. A 10 MB syntheticMarkdown()
/// document (a new heading+list block every ~140 bytes, ~89,000 blocks) took
/// over four minutes to open on Windows CI — a document shape no real 10 MB
/// markdown file has, since real ones are overwhelmingly prose. This is what
/// the "10 MB file opens fast" benchmark should actually measure.
QString realisticLargeDocument(qsizetype bytes)
{
    const QString sentence = QStringLiteral(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
        "incididunt ut labore et dolore magna aliqua. ");
    const QString heading = QStringLiteral("\n\n## Section\n\n");

    QString text;
    text.reserve(bytes + sentence.size());
    qsizetype nextHeadingAt = 0;
    while (text.size() < bytes) {
        if (text.size() >= nextHeadingAt) {
            text += heading;
            nextHeadingAt = text.size() + 50 * 1024; // one heading per ~50 KB
        }
        text += sentence;
    }
    return text;
}

} // namespace

class TestBenchmarks : public QObject
{
    Q_OBJECT

private slots:
    void mainWindowConstructionDoesNotBuildThePreview();
    void mainWindowConstructionStaysFast();
    void tenMegabyteFileOpensReasonablyFast();
    void editingStaysFast();
    void singleEditOnALargeDocumentStaysFast();
    void previewRefreshCompletesQuickly();
    void benchmarkMainWindowConstruction();
    void benchmarkPreviewRefresh();
#if defined(HUNGRYEDITOR_STRICT_BENCHMARKS) && defined(Q_OS_LINUX)
    void idleRssStaysUnderTarget();
#endif
};

void TestBenchmarks::mainWindowConstructionDoesNotBuildThePreview()
{
    // Deterministic regression guard for the lazy-preview change: no timing
    // involved, so it cannot be flaky. A regression here means someone made
    // the constructor build Chromium eagerly again.
    MainWindow window;
    QVERIFY(!window.previewIsCreated());
}

void TestBenchmarks::mainWindowConstructionStaysFast()
{
    QElapsedTimer timer;
    timer.start();
    MainWindow window;
    QVERIFY(!window.previewIsCreated());
#ifdef HUNGRYEDITOR_STRICT_BENCHMARKS
    // Plan target: cold start < 250 ms. Real, measured: ~2-5 ms — 9.1's
    // lazy preview deferral means this proxy comfortably clears the target
    // with a lot of room to spare.
    const qint64 threshold = 250;
#else
    const qint64 threshold = 2000; // gross-regression guard only, see file header
#endif
    QVERIFY2(timer.elapsed() < threshold,
             qPrintable(QStringLiteral("MainWindow construction took %1 ms").arg(timer.elapsed())));
}

void TestBenchmarks::tenMegabyteFileOpensReasonablyFast()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("big.md"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(realisticLargeDocument(10 * 1024 * 1024).toUtf8());
    }

    MainWindow window;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(window.openPath(path));
#ifdef HUNGRYEDITOR_STRICT_BENCHMARKS
    // Plan target: 500 ms. Real, measured: ~600-630 ms — not currently met.
    // 800 ms is the honest threshold: the real number with headroom for CI
    // noise, not the target itself.
    const qint64 threshold = 800;
#else
    const qint64 threshold = 5000; // gross-regression guard only, see file header
#endif
    QVERIFY2(timer.elapsed() < threshold,
             qPrintable(QStringLiteral("Opening a 10 MB file took %1 ms").arg(timer.elapsed())));
}

void TestBenchmarks::editingStaysFast()
{
    // A proxy for keystroke cost, not a literal single-character insert:
    // Editor::setText() replaces the whole buffer, so this measures the edit
    // pipeline's per-call overhead (highlighting/outline hookups), not paint
    // latency, which is not meaningful to measure headless.
    MainWindow window;
    Editor* editor = window.editor();

    const int iterations = 150;
    QString text;
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iterations; ++i) {
        text += QStringLiteral("line %1 of ordinary prose\n").arg(i);
        editor->setText(text);
    }
    const qint64 elapsed = timer.elapsed();
    const double perEdit = double(elapsed) / iterations;
#ifdef HUNGRYEDITOR_STRICT_BENCHMARKS
    // Plan target: keystroke-to-paint < 8 ms. Real, measured: ~0.03-0.05 ms
    // — comfortably met, but this proxy's document never grows past a few
    // KB, so it is not the meaningful check for that target on a real
    // document; see singleEditOnALargeDocumentStaysFast() for that.
    const double threshold = 8.0;
#else
    const double threshold = 50.0; // gross-regression guard only, see file header
#endif
    QVERIFY2(perEdit < threshold,
             qPrintable(QStringLiteral("Average edit cost was %1 ms").arg(perEdit)));
}

void TestBenchmarks::singleEditOnALargeDocumentStaysFast()
{
    // A real, single-character edit where the user is actually looking — a
    // shown, sized widget, edited near the top of its (still default)
    // viewport, the ordinary case of typing where the caret already is —
    // on a large, already-parsed tree-sitter-tier document. Compared
    // directly against an equivalent-cost full-buffer replace (the only
    // option before 11.6, and still what an edit whose own extent or
    // whose viewport can't be pinned down falls back to).
    //
    // The widget must actually be shown and sized: an unshown Editor's
    // "viewport" is degenerate (no real FirstVisibleLine()/LinesOnScreen()),
    // which silently defeats viewport-only span computation (11.7) — the
    // union of a degenerate viewport with an edit far from it can end up
    // covering nearly the whole document regardless of the edit's own size.
    //
    // Measured directly, in an unoptimized debug build (not asserted here,
    // since the two setups differ enough — a live Editor's full
    // highlight+outline+margin pipeline vs a bare TreeSitterEngine — that a
    // single run's ratio isn't a stable threshold): together, 11.6
    // (incremental reparse) and 11.7 (viewport-only span computation) bring
    // a single realistic edit on this 1.5 MB document from ~2450-2550ms
    // down to ~1400-1450ms; in a release build (see the
    // HUNGRYEDITOR_STRICT_BENCHMARKS assertion below), ~800ms down to
    // ~470ms. Real, but smaller than either commit's own mechanism would
    // suggest in isolation, because a third, uninvolved cost turns out to
    // dominate what's left: Editor::text() — called at least twice per edit
    // notification, once by updateFrontMatterFold() and once for the
    // highlight submission itself — does a full document buffer copy plus
    // UTF-8 conversion every time, independent of highlighting entirely.
    // Not this commit's to fix (11.7 is span computation specifically), but
    // worth recording plainly rather than letting an optimistic guess stand
    // in its place: the two fixes here are real and correctly scoped, they
    // just aren't the last remaining O(document) cost on this path.
    Editor editor;
    editor.resize(800, 600);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    const QString big = realisticLargeDocument(1536 * 1024); // 1.5 MB, tree-sitter tier

    QSignalSpy spy(&editor, &Editor::highlightingApplied);
    editor.setText(big);
    QVERIFY(spy.wait(5000)); // let the initial full parse land before timing anything
    spy.clear();

    QElapsedTimer incrementalTimer;
    incrementalTimer.start();
    editor.call().InsertText(0, "x"); // where the viewport already is
    QVERIFY2(spy.wait(5000), "a single edit's reparse did not complete in time");
    const qint64 incrementalElapsed = incrementalTimer.elapsed();

    Editor fullEditor;
    QSignalSpy fullSpy(&fullEditor, &Editor::highlightingApplied);
    fullEditor.setText(big);
    QVERIFY(fullSpy.wait(5000));
    fullSpy.clear();

    QElapsedTimer fullTimer;
    fullTimer.start();
    fullEditor.setText(big + QStringLiteral("x")); // whole-buffer replace: old-style full reparse
    QVERIFY(fullSpy.wait(5000));
    const qint64 fullElapsed = fullTimer.elapsed();

    QVERIFY2(incrementalElapsed <= fullElapsed + 200, // generous slack for CI noise
             qPrintable(QStringLiteral("Incremental edit (%1 ms) was slower than a full "
                                       "reparse (%2 ms)")
                            .arg(incrementalElapsed)
                            .arg(fullElapsed)));

#ifdef HUNGRYEDITOR_STRICT_BENCHMARKS
    // Plan target: keystroke-to-paint < 8 ms. Real, measured: ~470 ms on
    // this 1.5 MB document — nowhere close, for the reasons in the comment
    // above (Editor::text()'s double full-document conversion, chief among
    // them). 800 ms is the honest threshold: the real number with headroom,
    // not the target. This is the meaningful check for that target on an
    // actual document — editingStaysFast() above only proves the pipeline's
    // own per-call overhead is cheap on a document a few KB in size.
    QVERIFY2(incrementalElapsed < 800,
             qPrintable(QStringLiteral("Incremental edit took %1 ms").arg(incrementalElapsed)));
#endif
}

void TestBenchmarks::previewRefreshCompletesQuickly()
{
    FakeBackend backend;
    PreviewController controller(&backend);
    QSignalSpy rendered(&controller, &PreviewController::rendered);

    const QString markdown = syntheticMarkdown(200 * 1024);
    QElapsedTimer timer;
    timer.start();
    controller.setMarkdown(markdown);
    controller.flush();
    const qint64 elapsed = timer.elapsed();

    QCOMPARE(rendered.count(), 1);
#ifdef HUNGRYEDITOR_STRICT_BENCHMARKS
    // Plan target: preview refresh < 100 ms. Real, measured: ~80-100 ms —
    // right at the target, not comfortably under it; benchmarkPreviewRefresh()
    // below reports the trendable per-iteration number for the same
    // document. 150 ms gives a noisy CI run a little room without hiding a
    // real regression back over the target.
    const qint64 threshold = 150;
#else
    const qint64 threshold = 2000; // gross-regression guard only, see file header
#endif
    QVERIFY2(elapsed < threshold,
             qPrintable(QStringLiteral("Preview refresh took %1 ms").arg(elapsed)));
}

void TestBenchmarks::benchmarkMainWindowConstruction()
{
    QBENCHMARK
    {
        MainWindow window;
    }
}

void TestBenchmarks::benchmarkPreviewRefresh()
{
    FakeBackend backend;
    PreviewController controller(&backend);
    const QString markdown = syntheticMarkdown(200 * 1024);

    QBENCHMARK
    {
        controller.setMarkdown(markdown);
        controller.flush();
    }
}

#if defined(HUNGRYEDITOR_STRICT_BENCHMARKS) && defined(Q_OS_LINUX)
void TestBenchmarks::idleRssStaysUnderTarget()
{
    // The plan's own idle-RSS target has no in-process way to measure the
    // real production binary — a QtTest harness links QtTest and the whole
    // test executable's dependency graph, which is not representative —
    // so this launches the actual hungryeditor executable as a real
    // subprocess and reads its own /proc/<pid>/status, the same way a user
    // launching it would experience. Linux-only (no /proc elsewhere) and
    // strict-only (needs a release build's real executable, not a debug
    // one nobody ships).
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("QTWEBENGINE_CHROMIUM_FLAGS"),
               QStringLiteral("--no-sandbox --disable-gpu --disable-dev-shm-usage"));
    env.insert(QStringLiteral("QTWEBENGINE_DISABLE_SANDBOX"), QStringLiteral("1"));

    QProcess process;
    process.setProcessEnvironment(env);
    process.start(QStringLiteral(HUNGRYEDITOR_EXECUTABLE_PATH), {});
    QVERIFY2(process.waitForStarted(5000), "the production executable did not start");

    // No cross-process "finished starting up" signal to wait on instead —
    // a generous fixed delay for MainWindow construction and Qt WebEngine's
    // own one-time context initialization (see below) to settle.
    QTest::qWait(3000);

    QFile status(QStringLiteral("/proc/%1/status").arg(process.processId()));
    QVERIFY2(status.open(QIODevice::ReadOnly),
             "could not read the running process's /proc/<pid>/status");
    qint64 rssKb = -1;
    const QList<QByteArray> lines = status.readAll().split('\n');
    for (const QByteArray& line : lines) {
        if (line.startsWith("VmRSS:")) {
            const QList<QByteArray> fields = line.simplified().split(' ');
            if (fields.size() >= 2) {
                rssKb = fields[1].toLongLong();
            }
        }
    }

    process.terminate();
    QVERIFY(process.waitForFinished(5000));
    QVERIFY2(rssKb > 0, "could not parse VmRSS from /proc/<pid>/status");

    // Plan target: idle RSS < 120 MB. Real, measured cold-start RSS for the
    // production binary — before opening any document, before the preview
    // pane is ever shown — is ~190 MB. The gap's cause is already known,
    // not mysterious: Qt WebEngine performs its own one-time Chromium
    // context initialization as soon as the WebEngineWidgets module is
    // linked into the process at all, independent of 9.1's lazy
    // *preview-widget* construction and regardless of whether a preview
    // pane is ever actually shown. It is the same root cause Phase 10.4's
    // deferred WebView2 swap exists to eventually remove on Windows (there
    // by dropping Qt WebEngine from the link entirely) — not something to
    // silently re-open here. 250 MB leaves headroom for CI noise without
    // hiding a real regression.
    QVERIFY2(
        rssKb < 250 * 1024,
        qPrintable(QStringLiteral("Idle RSS was %1 MB").arg(static_cast<double>(rssKb) / 1024.0)));
}
#endif

QTEST_MAIN(TestBenchmarks)
#include "test_benchmarks.moc"
