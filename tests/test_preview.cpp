// Coverage for the preview backend and its Qt WebEngine implementation.

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

QTEST_MAIN(TestPreview)
#include "test_preview.moc"
