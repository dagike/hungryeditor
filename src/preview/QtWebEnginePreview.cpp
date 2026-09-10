#include "preview/QtWebEnginePreview.h"

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
<link rel="stylesheet" href="qrc:///hungryeditor/preview/katex.min.css">
<style id="he-theme"></style>
</head>
<body>
<div id="hungryeditor-content"></div>
<script src="qrc:///qtwebchannel/qwebchannel.js"></script>
<script src="qrc:///hungryeditor/preview/mermaid.min.js"></script>
<script src="qrc:///hungryeditor/preview/katex.min.js"></script>
<script>
  "use strict";

  var mermaidReady = typeof mermaid !== "undefined";
  if (mermaidReady) {
    mermaid.initialize({ startOnLoad: false, securityLevel: "strict", theme: "neutral" });
  }

  var katexReady = typeof katex !== "undefined";

  window.addEventListener("load", function () {
    new QWebChannel(qt.webChannelTransport, function (channel) {
      var bridge = channel.objects.bridge;
      var target = document.getElementById("hungryeditor-content");

      // Bumped on every apply() so a diagram that finishes rendering after the
      // body has moved on is dropped instead of painted over fresh content.
      var renderToken = 0;

      function renderMermaid() {
        if (!mermaidReady) return;
        var token = ++renderToken;
        var codes = target.querySelectorAll("code.language-mermaid");
        for (var i = 0; i < codes.length; i++) {
          (function (code, index) {
            var pre = code.closest("pre");
            if (!pre) return;
            var holder = document.createElement("div");
            holder.className = "mermaid-diagram";
            if (pre.hasAttribute("data-src-line")) {
              holder.setAttribute("data-src-line", pre.getAttribute("data-src-line"));
            }
            pre.replaceWith(holder);
            mermaid.render("he-mermaid-" + token + "-" + index, code.textContent).then(
              function (out) { if (token === renderToken) holder.innerHTML = out.svg; },
              function (err) {
                if (token === renderToken) holder.textContent = String((err && err.message) || err);
              });
          })(codes[i], i);
        }
      }

      function renderMath() {
        if (!katexReady) return;
        var spans = target.querySelectorAll(".math-inline, .math-display");
        for (var i = 0; i < spans.length; i++) {
          var span = spans[i];
          if (span.dataset.rendered) continue;
          span.dataset.rendered = "1";
          katex.render(span.textContent, span, {
            displayMode: span.classList.contains("math-display"),
            throwOnError: false
          });
        }
      }

      function apply(html) { target.innerHTML = html; renderMermaid(); renderMath(); }
      function applyTheme(css) { document.getElementById("he-theme").textContent = css; }

      function blocks() { return target.querySelectorAll("[data-src-line]"); }
      function lineOf(el) { return parseInt(el.getAttribute("data-src-line"), 10) || 0; }

      // Ignore the scroll events our own scrollToLine() triggers.
      var muteReportUntil = 0;

      function scrollToLine(line) {
        var list = blocks();
        var chosen = null;
        for (var i = 0; i < list.length; i++) {
          if (lineOf(list[i]) >= line) { chosen = list[i]; break; }
        }
        muteReportUntil = Date.now() + 250;
        var y = chosen ? Math.max(0, chosen.offsetTop - 8) : document.body.scrollHeight;
        window.scrollTo(0, y);
      }

      function reportScroll() {
        if (Date.now() < muteReportUntil) return;
        var list = blocks();
        var line = 0;
        for (var i = 0; i < list.length; i++) {
          if (list[i].getBoundingClientRect().top <= 4) {
            line = lineOf(list[i]);
          } else {
            break;
          }
        }
        bridge.reportScroll(line);
      }

      function reportHeadingClick(event) {
        var el = event.target.closest("h1, h2, h3, h4, h5, h6");
        if (el && el.hasAttribute("data-src-line")) {
          bridge.reportClick(lineOf(el));
        }
      }

      function reportTaskToggle(event) {
        var box = event.target;
        if (box.tagName !== "INPUT" || box.type !== "checkbox") return;
        var li = box.closest("[data-src-line]");
        if (li) bridge.reportTaskToggle(lineOf(li), box.checked);
      }

      bridge.contentChanged.connect(apply);
      bridge.themeCssChanged.connect(applyTheme);
      bridge.scrollToLineRequested.connect(scrollToLine);
      window.addEventListener("scroll", reportScroll, { passive: true });
      target.addEventListener("click", reportHeadingClick);
      target.addEventListener("change", reportTaskToggle);

      applyTheme(bridge.themeCss);
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
    : PreviewBackend(parent), view_(std::make_unique<QWebEngineView>()),
      bridge_(new PreviewBridge(this)), channel_(new QWebChannel(this))
{
    channel_->registerObject(QStringLiteral("bridge"), bridge_);
    view_->page()->setWebChannel(channel_);

    connect(view_.get(), &QWebEngineView::loadFinished, this, &PreviewBackend::loadFinished);
    connect(bridge_, &PreviewBridge::pageReady, this, &PreviewBackend::ready);
    connect(bridge_, &PreviewBridge::viewerScrolled, this, &PreviewBackend::scrolledToSourceLine);
    connect(bridge_, &PreviewBridge::headingClicked, this, &PreviewBackend::clickedSourceLine);
    connect(bridge_, &PreviewBridge::taskToggled, this, &PreviewBackend::taskToggled);
}

QtWebEnginePreview::~QtWebEnginePreview() = default;

QWidget* QtWebEnginePreview::widget()
{
    return view_.get();
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
                                       const std::function<void(const QVariant&)>& callback)
{
    if (callback) {
        view_->page()->runJavaScript(script, callback);
    } else {
        view_->page()->runJavaScript(script);
    }
}

void QtWebEnginePreview::setThemeCss(const QString& css)
{
    bridge_->setThemeCss(css);
}

void QtWebEnginePreview::scrollToSourceLine(int line)
{
    bridge_->requestScrollToLine(line);
}

} // namespace hungryeditor
