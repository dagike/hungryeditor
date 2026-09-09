// Coverage for the preview backend and its Qt WebEngine implementation.

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QtTest>

#include "preview/QtWebEnginePreview.h"

using hungryeditor::PreviewBackend;
using hungryeditor::QtWebEnginePreview;

class TestPreview : public QObject
{
    Q_OBJECT

private slots:
    void exposesAnEmbeddableWidget();
    void rendersHtmlAndEvaluatesScript();
    void streamsContentThroughTheBridge();
};

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

QTEST_MAIN(TestPreview)
#include "test_preview.moc"
