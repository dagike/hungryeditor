// Coverage for the preview backend and its Qt WebEngine implementation.

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QtTest>

#include "preview/QtWebEnginePreview.h"
#include "theme/Theme.h"

using hungryeditor::PreviewBackend;
using hungryeditor::QtWebEnginePreview;
using hungryeditor::Theme;

class TestPreview : public QObject
{
    Q_OBJECT

private slots:
    void exposesAnEmbeddableWidget();
    void rendersHtmlAndEvaluatesScript();
    void streamsContentThroughTheBridge();
    void scrollToSourceLineMovesTheViewport();
    void scrollingThePageReportsASourceLine();
    void themeCssStylesTheRenderedContent();
    void clickingAHeadingReportsItsSourceLine();
    void togglingATaskCheckboxReportsItsLineAndState();
    void rendersABundledMermaidDiagram();
};

namespace {

/// Evaluate `script` and block (pumping the event loop) until the result lands.
QVariant evalJs(hungryeditor::QtWebEnginePreview& preview, const QString& script)
{
    QVariant result;
    bool done = false;
    preview.runJavaScript(script, [&](const QVariant& value) {
        result = value;
        done = true;
    });
    QElapsedTimer clock;
    clock.start();
    while (!done && clock.elapsed() < 5000) {
        QTest::qWait(20);
    }
    return result;
}

/// A tall document: 60 paragraphs, each tagged with its source line.
QString tallBody()
{
    QString html;
    for (int i = 0; i < 60; ++i) {
        html += QStringLiteral("<p data-src-line=\"%1\">Paragraph number %1 with enough text to "
                               "take a full line of the preview pane.</p>")
                    .arg(i);
    }
    return html;
}

} // namespace

void TestPreview::exposesAnEmbeddableWidget()
{
    QtWebEnginePreview preview;
    QVERIFY(preview.widget() != nullptr);
}

void TestPreview::rendersHtmlAndEvaluatesScript()
{
    QtWebEnginePreview preview;
    QSignalSpy loaded(&preview, &PreviewBackend::loadFinished);

    preview.setHtml(QStringLiteral(
        "<!doctype html><meta charset=utf-8><body><p id=\"m\">hello preview</p></body>"));

    QVERIFY(loaded.wait(20000));
    QCOMPARE(loaded.last().at(0).toBool(), true);

    QString text;
    bool answered = false;
    preview.runJavaScript(QStringLiteral("document.getElementById('m').textContent"),
                          [&](const QVariant& value) {
                              text = value.toString();
                              answered = true;
                          });
    QTRY_VERIFY_WITH_TIMEOUT(answered, 10000);
    QCOMPARE(text, QStringLiteral("hello preview"));
}

void TestPreview::streamsContentThroughTheBridge()
{
    QtWebEnginePreview preview;
    QSignalSpy ready(&preview, &PreviewBackend::ready);

    preview.setContent(QStringLiteral("<h1 data-src-line=\"3\">Streamed heading</h1>"));
    QVERIFY(ready.wait(20000)); // shell loaded and the channel handshook

    QString innerHtml;
    bool answered = false;
    preview.runJavaScript(
        QStringLiteral("document.getElementById('hungryeditor-content').innerHTML"),
        [&](const QVariant& value) {
            innerHtml = value.toString();
            answered = true;
        });
    QTRY_VERIFY_WITH_TIMEOUT(answered, 10000);
    QVERIFY(innerHtml.contains(QStringLiteral("Streamed heading")));

    // A second push replaces the body without another shell load.
    QSignalSpy loads(&preview, &PreviewBackend::loadFinished);
    preview.setContent(QStringLiteral("<p>Replaced</p>"));

    QString updated;
    QElapsedTimer clock;
    clock.start();
    while (!updated.contains(QStringLiteral("Replaced")) && clock.elapsed() < 10000) {
        bool got = false;
        preview.runJavaScript(
            QStringLiteral("document.getElementById('hungryeditor-content').innerHTML"),
            [&](const QVariant& value) {
                updated = value.toString();
                got = true;
            });
        QTRY_VERIFY(got);
    }
    QVERIFY(updated.contains(QStringLiteral("Replaced")));
    QCOMPARE(loads.count(), 0); // no page reload for a content swap
}

void TestPreview::scrollToSourceLineMovesTheViewport()
{
    QtWebEnginePreview preview;
    preview.widget()->resize(360, 200);
    preview.widget()->show();

    QSignalSpy ready(&preview, &PreviewBackend::ready);
    preview.setContent(tallBody());
    QVERIFY(ready.wait(20000));

    QCOMPARE(evalJs(preview, QStringLiteral("Math.round(window.scrollY)")).toInt(), 0);

    preview.scrollToSourceLine(40);

    int scrollY = 0;
    QElapsedTimer clock;
    clock.start();
    while (scrollY == 0 && clock.elapsed() < 10000) {
        scrollY = evalJs(preview, QStringLiteral("Math.round(window.scrollY)")).toInt();
    }
    QVERIFY(scrollY > 0);
}

void TestPreview::scrollingThePageReportsASourceLine()
{
    QtWebEnginePreview preview;
    preview.widget()->resize(360, 200);
    preview.widget()->show();

    QSignalSpy ready(&preview, &PreviewBackend::ready);
    preview.setContent(tallBody());
    QVERIFY(ready.wait(20000));

    QSignalSpy scrolled(&preview, &PreviewBackend::scrolledToSourceLine);
    evalJs(preview, QStringLiteral("window.scrollTo(0, document.body.scrollHeight); void 0"));

    QElapsedTimer clock;
    clock.start();
    while (scrolled.isEmpty() && clock.elapsed() < 5000) {
        QTest::qWait(50);
    }
    QVERIFY(!scrolled.isEmpty());
    QVERIFY(scrolled.last().at(0).toInt() > 0);
}

void TestPreview::themeCssStylesTheRenderedContent()
{
    QtWebEnginePreview preview;
    preview.setThemeCss(Theme::builtin().previewCss());

    QSignalSpy ready(&preview, &PreviewBackend::ready);
    preview.setContent(QStringLiteral("<h1 data-src-line=\"0\">Themed heading</h1>"));
    QVERIFY(ready.wait(20000));

    const QString color =
        evalJs(preview, QStringLiteral("getComputedStyle(document.querySelector('h1')).color"))
            .toString();
    QCOMPARE(color, QStringLiteral("rgb(5, 80, 174)")); // #0550ae, the theme heading colour
}

void TestPreview::clickingAHeadingReportsItsSourceLine()
{
    QtWebEnginePreview preview;
    QSignalSpy ready(&preview, &PreviewBackend::ready);
    preview.setContent(QStringLiteral("<h2 data-src-line=\"7\">Clickable</h2>"
                                      "<p data-src-line=\"9\">not a heading</p>"));
    QVERIFY(ready.wait(20000));

    QSignalSpy clicked(&preview, &PreviewBackend::clickedSourceLine);
    evalJs(preview, QStringLiteral("document.querySelector('p').click(); void 0"));
    QTest::qWait(200);
    QVERIFY(clicked.isEmpty()); // paragraphs are not clickable

    evalJs(preview, QStringLiteral("document.querySelector('h2').click(); void 0"));
    QElapsedTimer clock;
    clock.start();
    while (clicked.isEmpty() && clock.elapsed() < 5000) {
        QTest::qWait(50);
    }
    QVERIFY(!clicked.isEmpty());
    QCOMPARE(clicked.last().at(0).toInt(), 7);
}

void TestPreview::togglingATaskCheckboxReportsItsLineAndState()
{
    QtWebEnginePreview preview;
    QSignalSpy ready(&preview, &PreviewBackend::ready);
    preview.setContent(
        QStringLiteral("<ul><li class=\"task-list-item\" data-src-line=\"5\">"
                       "<input type=\"checkbox\" class=\"task-checkbox\"> pick up milk</li></ul>"));
    QVERIFY(ready.wait(20000));

    QSignalSpy toggled(&preview, &PreviewBackend::taskToggled);
    evalJs(preview,
           QStringLiteral("document.querySelector('input.task-checkbox').click(); void 0"));

    QElapsedTimer clock;
    clock.start();
    while (toggled.isEmpty() && clock.elapsed() < 5000) {
        QTest::qWait(50);
    }
    QVERIFY(!toggled.isEmpty());
    QCOMPARE(toggled.last().at(0).toInt(), 5);
    QCOMPARE(toggled.last().at(1).toBool(), true);
}

void TestPreview::rendersABundledMermaidDiagram()
{
    QtWebEnginePreview preview;
    preview.widget()->resize(400, 300);
    preview.widget()->show();

    QSignalSpy ready(&preview, &PreviewBackend::ready);
    preview.setContent(
        QStringLiteral("<pre data-src-line=\"2\"><code class=\"language-mermaid\">graph TD; "
                       "A--&gt;B;</code></pre>"));
    QVERIFY(ready.wait(20000));

    // mermaid parses a 3 MB bundle then renders asynchronously.
    QString probe;
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < 20000) {
        probe =
            evalJs(preview, QStringLiteral("(function () {"
                                           "  var d = document.querySelector('.mermaid-diagram');"
                                           "  if (!d || !d.querySelector('svg')) return '';"
                                           "  return d.getAttribute('data-src-line') || 'none';"
                                           "})()"))
                .toString();
        if (!probe.isEmpty()) {
            break;
        }
        QTest::qWait(100);
    }
    QCOMPARE(probe, QStringLiteral("2")); // rendered, and the source line carried over
    QVERIFY(
        !evalJs(preview, QStringLiteral("document.querySelector('code.language-mermaid') != null"))
             .toBool()); // the <pre> was swapped out
}

QTEST_MAIN(TestPreview)
#include "test_preview.moc"
