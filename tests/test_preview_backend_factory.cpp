// Coverage for the preview engine selection logic in isolation from actually
// constructing a backend (which would mean bringing up Chromium here too;
// test_preview and test_main_window already exercise the real thing
// end-to-end). HUNGRYEDITOR_PREVIEW_ENGINE is unset/restored around every
// case so this test cannot leak state into another one in the same run.

#include <QtTest>

#include "preview/PreviewBackendFactory.h"

using hungryeditor::isWebView2Available;
using hungryeditor::PreviewEngine;
using hungryeditor::selectedPreviewEngine;

class TestPreviewBackendFactory : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void webView2IsNeverAvailableYet();
    void defaultsToQtWebEngineWhenUnset();
    void defaultsToQtWebEngineForAnUnrecognisedValue();
    void staysOnQtWebEngineWhenWebView2IsRequestedButUnavailable();
    void honorsAnExplicitQtWebEngineSelection();
};

void TestPreviewBackendFactory::cleanup()
{
    qunsetenv("HUNGRYEDITOR_PREVIEW_ENGINE");
}

void TestPreviewBackendFactory::webView2IsNeverAvailableYet()
{
    // The COM backend is a follow-on branch, not yet merged -- this must stay
    // false on every platform, including Windows, until that lands.
    QCOMPARE(isWebView2Available(), false);
}

void TestPreviewBackendFactory::defaultsToQtWebEngineWhenUnset()
{
    qunsetenv("HUNGRYEDITOR_PREVIEW_ENGINE");
    QCOMPARE(selectedPreviewEngine(), PreviewEngine::QtWebEngine);
}

void TestPreviewBackendFactory::defaultsToQtWebEngineForAnUnrecognisedValue()
{
    qputenv("HUNGRYEDITOR_PREVIEW_ENGINE", "not-a-real-engine");
    QCOMPARE(selectedPreviewEngine(), PreviewEngine::QtWebEngine);
}

void TestPreviewBackendFactory::staysOnQtWebEngineWhenWebView2IsRequestedButUnavailable()
{
    qputenv("HUNGRYEDITOR_PREVIEW_ENGINE", "webview2");
    // isWebView2Available() is unconditionally false today, so the override
    // must be refused rather than selecting an engine nothing can build.
    QCOMPARE(selectedPreviewEngine(), PreviewEngine::QtWebEngine);
}

void TestPreviewBackendFactory::honorsAnExplicitQtWebEngineSelection()
{
    qputenv("HUNGRYEDITOR_PREVIEW_ENGINE", "qtwebengine");
    QCOMPARE(selectedPreviewEngine(), PreviewEngine::QtWebEngine);
}

QTEST_APPLESS_MAIN(TestPreviewBackendFactory)
#include "test_preview_backend_factory.moc"
