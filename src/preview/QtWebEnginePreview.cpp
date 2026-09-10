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
<!-- The preview never needs the network: scripts, styles and fonts are all
     bundled under qrc:, images arrive inlined as data: URIs. This policy makes
     the browser enforce that, so a stray remote src in the rendered markdown
     (a tracking pixel, a pasted <img>) is refused before a request goes out.
     'unsafe-inline' covers the shell's own inline <style>/<script> and the
     inline style attributes KaTeX writes; it is not a sandbox for our bundled
     JS, only a wall against exfiltration. -->
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src qrc: 'unsafe-inline'; style-src qrc: 'unsafe-inline'; img-src qrc: data:; font-src qrc:; connect-src 'none'; base-uri 'none'; form-action 'none'">
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
      var lazyObserver = null;

      // Swap a failed diagram/math node for a labelled error surface, keeping
      // its source line so scroll-sync still lands on it. Inline math gets an
      // inline span so we never nest a <div> inside a <p>.
      function renderError(el, kind, err, block) {
        var message = String((err && err.message) || err || "unknown error");
        if (!block) {
          var span = document.createElement("span");
          span.className = "he-render-error-inline";
          span.title = kind + " error: " + message;
          span.textContent = el.textContent;
          el.replaceWith(span);
          return;
        }
        var box = document.createElement("div");
        box.className = "he-render-error";
        if (el.hasAttribute("data-src-line")) {
          box.setAttribute("data-src-line", el.getAttribute("data-src-line"));
        }
        var label = document.createElement("strong");
        label.textContent = kind + " error";
        var body = document.createElement("pre");
        body.textContent = message;
        box.appendChild(label);
        box.appendChild(body);
        el.replaceWith(box);
      }

      function renderMermaidHolder(holder) {
        if (!mermaidReady) { holder.classList.remove("mermaid-pending"); return; }
        var id = holder.dataset.mermaidId;
        var src = holder.dataset.src || "";
        function fail(err) {
          renderError(holder, "Diagram", err, true);
          var orphan = document.getElementById("d" + id);
          if (orphan) orphan.remove();
        }
        try {
          mermaid.render(id, src).then(function (out) {
            if (!holder.isConnected) return;
            holder.classList.remove("mermaid-pending");
            holder.innerHTML = out.svg;
          }, fail);
        } catch (err) {
          fail(err);
        }
      }

      function renderMathSpan(span) {
        if (!katexReady) return;
        var display = span.classList.contains("math-display");
        try {
          katex.render(span.textContent, span, { displayMode: display, throwOnError: true });
        } catch (err) {
          renderError(span, "Math", err, display);
        }
      }

      function renderLazily(el) {
        if (el.dataset.heRendered) return;
        el.dataset.heRendered = "1";
        if (el.classList.contains("mermaid-diagram")) {
          renderMermaidHolder(el);
        } else {
          renderMathSpan(el);
        }
      }

      // Diagrams and math are the slow part of a render — a page with dozens
      // would stall on apply(). Swap each mermaid fence for a stable holder up
      // front (so scroll-sync keeps its source line) but defer the actual
      // mermaid/katex work until the block nears the viewport.
      function scheduleRenders() {
        var token = ++renderToken;
        if (lazyObserver) { lazyObserver.disconnect(); lazyObserver = null; }

        var codes = target.querySelectorAll("code.language-mermaid");
        for (var i = 0; i < codes.length; i++) {
          var pre = codes[i].closest("pre");
          if (!pre) continue;
          var holder = document.createElement("div");
          holder.className = "mermaid-diagram mermaid-pending";
          if (pre.hasAttribute("data-src-line")) {
            holder.setAttribute("data-src-line", pre.getAttribute("data-src-line"));
          }
          holder.dataset.src = codes[i].textContent;
          holder.dataset.mermaidId = "he-mermaid-" + token + "-" + i;
          pre.replaceWith(holder);
        }

        var items = target.querySelectorAll(".mermaid-diagram, .math-inline, .math-display");
        if (!("IntersectionObserver" in window)) {
          for (var j = 0; j < items.length; j++) renderLazily(items[j]);
          return;
        }
        lazyObserver = new IntersectionObserver(function (entries) {
          for (var k = 0; k < entries.length; k++) {
            if (entries[k].isIntersecting) {
              lazyObserver.unobserve(entries[k].target);
              renderLazily(entries[k].target);
            }
          }
        }, { rootMargin: "800px 0px" });
        for (var m = 0; m < items.length; m++) lazyObserver.observe(items[m]);
      }

      function apply(html) { target.innerHTML = html; scheduleRenders(); }
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
