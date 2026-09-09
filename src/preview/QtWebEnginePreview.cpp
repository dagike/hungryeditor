#include "preview/QtWebEnginePreview.h"

#include <utility>

#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineView>

#include "preview/PreviewBridge.h"

namespace hungryeditor {

namespace {

// The preview shell: loaded once, then setContent() streams the rendered body
// into #hungryeditor-content over the QWebChannel. A qrc: base URL lets the
// page pull Qt WebEngine's bundled qwebchannel.js.
const QUrl kShellBaseUrl(QStringLiteral("qrc:/hungryeditor/preview/"));

const char* const kShellHtml = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>Preview</title>
<style>
  html { box-sizing: border-box; }
  *, *::before, *::after { box-sizing: inherit; }
  body {
    margin: 0;
    padding: 1.25rem 1.5rem;
    font: 15px/1.6 -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  }
  #hungryeditor-content > :first-child { margin-top: 0; }
</style>
</head>
<body>
<div id="hungryeditor-content"></div>
<script src="qrc:///qtwebchannel/qwebchannel.js"></script>
<script>
  "use strict";
  window.addEventListener("load", function () {
    new QWebChannel(qt.webChannelTransport, function (channel) {
      var bridge = channel.objects.bridge;
      var target = document.getElementById("hungryeditor-content");
      function apply(html) { target.innerHTML = html; }
      bridge.contentChanged.connect(apply);
      apply(bridge.content);
      bridge.notifyReady();
    });
  });
</script>
</body>
</html>
)HTML";

} // namespace

QtWebEnginePreview::QtWebEnginePreview(QObject* parent)
    : PreviewBackend(parent), view_(new QWebEngineView), bridge_(new PreviewBridge(this)),
      channel_(new QWebChannel(this))
{
    channel_->registerObject(QStringLiteral("bridge"), bridge_);
    view_->page()->setWebChannel(channel_);

    connect(view_.data(), &QWebEngineView::loadFinished, this, &PreviewBackend::loadFinished);
    connect(bridge_, &PreviewBridge::pageReady, this, &PreviewBackend::ready);
}

QtWebEnginePreview::~QtWebEnginePreview()
{
    // QPointer is null if the view was embedded and destroyed with its parent.
    delete view_.data();
}

QWidget* QtWebEnginePreview::widget()
{
    return view_.data();
}

void QtWebEnginePreview::setHtml(const QString& html, const QUrl& baseUrl)
{
    shellRequested_ = false; // a fresh full document replaces the shell
    view_->setHtml(html, baseUrl);
}

void QtWebEnginePreview::setContent(const QString& bodyHtml, const QUrl& baseUrl)
{
    baseUrl_ = baseUrl;
    bridge_->setContent(bodyHtml); // seed before the shell's script reads it

    if (!shellRequested_) {
        shellRequested_ = true;
        view_->setHtml(QString::fromUtf8(kShellHtml), kShellBaseUrl);
    }
}

void QtWebEnginePreview::runJavaScript(const QString& script,
                                       std::function<void(const QVariant&)> callback)
{
    if (callback) {
        view_->page()->runJavaScript(script, std::move(callback));
    } else {
        view_->page()->runJavaScript(script);
    }
}

} // namespace hungryeditor
