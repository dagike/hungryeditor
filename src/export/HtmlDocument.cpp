#include "export/HtmlDocument.h"

#include <QFile>
#include <QList>
#include <QRegularExpression>

#include "markdown/ImageResolver.h"
#include "markdown/Md4cRenderer.h"

namespace hungryeditor::htmlexport {

namespace {

QString readAsset(const QString& qrcPath)
{
    QFile file(qrcPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QString escapeHtml(const QString& text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return escaped;
}

// katex.min.css asks each @font-face for a woff2/woff/ttf trio; only the
// woff2 files are vendored, so the woff/ttf alternates are dropped and the
// woff2 itself is inlined as a data: URI — the export must carry no file
// references at all.
QString inlineKatexFonts(QString css)
{
    static const QRegularExpression kDropFallbacks(
        QStringLiteral(R"(,url\(fonts/[^)]+\.(?:woff|ttf)\)\s*format\([^)]*\))"));
    css.remove(kDropFallbacks);

    static const QRegularExpression kFontRef(QStringLiteral(R"(url\(fonts/([^)]+\.woff2)\))"));
    QList<QRegularExpressionMatch> matches;
    QRegularExpressionMatchIterator it = kFontRef.globalMatch(css);
    while (it.hasNext()) {
        matches.append(it.next());
    }

    // Replace back to front so earlier match offsets stay valid.
    for (auto matchIt = matches.crbegin(); matchIt != matches.crend(); ++matchIt) {
        QFile file(QStringLiteral(":/hungryeditor/preview/fonts/") + matchIt->captured(1));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString dataUri = QStringLiteral("url(data:font/woff2;base64,") +
                                QString::fromLatin1(file.readAll().toBase64()) +
                                QStringLiteral(")");
        css.replace(matchIt->capturedStart(0), matchIt->capturedLength(0), dataUri);
    }
    return css;
}

const char* const kBaseCss = R"CSS(
html { box-sizing: border-box; }
*, *::before, *::after { box-sizing: inherit; }
body {
  margin: 0 auto;
  max-width: 52rem;
  padding: 1.25rem 1.5rem;
  font: 15px/1.6 -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
}
.markdown-body > :first-child { margin-top: 0; }
)CSS";

// Trimmed, synchronous versions of the live preview's render pass: no
// QWebChannel, no lazy IntersectionObserver — an exported file just renders
// everything on load. Kept as two independent scripts (rather than one
// combined pass) so a document needing only one library never mentions the
// other.
const char* const kMermaidRenderScript = R"JS(
(function () {
  "use strict";
  if (typeof mermaid === "undefined") return;
  mermaid.initialize({ startOnLoad: false, securityLevel: "strict", theme: "neutral" });

  function renderError(el, err) {
    var box = document.createElement("div");
    box.className = "he-render-error";
    var label = document.createElement("strong");
    label.textContent = "Diagram error";
    var body = document.createElement("pre");
    body.textContent = String((err && err.message) || err || "unknown error");
    box.appendChild(label);
    box.appendChild(body);
    el.replaceWith(box);
  }

  var codes = document.querySelectorAll("code.language-mermaid");
  for (var i = 0; i < codes.length; i++) {
    var pre = codes[i].closest("pre");
    if (!pre) continue;
    var holder = document.createElement("div");
    holder.className = "mermaid-diagram";
    var id = "he-mermaid-" + i;
    var src = codes[i].textContent;
    pre.replaceWith(holder);
    (function (holder, id, src) {
      try {
        mermaid.render(id, src).then(function (out) {
          holder.innerHTML = out.svg;
        }, function (err) {
          renderError(holder, err);
        });
      } catch (err) {
        renderError(holder, err);
      }
    })(holder, id, src);
  }
})();
)JS";

const char* const kKatexRenderScript = R"JS(
(function () {
  "use strict";
  if (typeof katex === "undefined") return;

  function renderError(el, err, display) {
    var span = document.createElement("span");
    span.className = "he-render-error-inline";
    span.title = "Math error: " + String((err && err.message) || err || "unknown error");
    span.textContent = el.textContent;
    if (display) {
      var box = document.createElement("div");
      box.className = "he-render-error";
      var label = document.createElement("strong");
      label.textContent = "Math error";
      var body = document.createElement("pre");
      body.textContent = String((err && err.message) || err || "unknown error");
      box.appendChild(label);
      box.appendChild(body);
      el.replaceWith(box);
      return;
    }
    el.replaceWith(span);
  }

  var spans = document.querySelectorAll(".math-inline, .math-display");
  for (var j = 0; j < spans.length; j++) {
    var span = spans[j];
    var display = span.classList.contains("math-display");
    try {
      katex.render(span.textContent, span, { displayMode: display, throwOnError: true });
    } catch (err) {
      renderError(span, err, display);
    }
  }
})();
)JS";

} // namespace

QString build(const QString& markdown, const Options& options)
{
    const QString fragment =
        images::inlineLocalImages(Md4cRenderer().toHtml(markdown), options.documentDir);

    const bool needsMath = fragment.contains(QLatin1String("math-inline")) ||
                           fragment.contains(QLatin1String("math-display"));
    const bool needsMermaid = fragment.contains(QLatin1String("language-mermaid"));

    QString html;
    html += QLatin1String("<!doctype html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n");
    html +=
        QLatin1String("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    html += QStringLiteral("<title>%1</title>\n").arg(escapeHtml(options.title));

    html += QLatin1String("<style>\n");
    html += QString::fromLatin1(kBaseCss);
    html += options.theme.previewCss();
    html += QLatin1String("\n</style>\n");

    if (needsMath) {
        html += QLatin1String("<style>\n");
        html += inlineKatexFonts(readAsset(QStringLiteral(":/hungryeditor/preview/katex.min.css")));
        html += QLatin1String("\n</style>\n");
    }

    html += QLatin1String("</head>\n<body>\n<article class=\"markdown-body\">\n");
    html += fragment;
    html += QLatin1String("\n</article>\n");

    if (needsMermaid) {
        html += QLatin1String("<script>\n");
        html += readAsset(QStringLiteral(":/hungryeditor/preview/mermaid.min.js"));
        html += QLatin1String("\n</script>\n");
    }
    if (needsMath) {
        html += QLatin1String("<script>\n");
        html += readAsset(QStringLiteral(":/hungryeditor/preview/katex.min.js"));
        html += QLatin1String("\n</script>\n");
    }
    if (needsMermaid) {
        html += QLatin1String("<script>\n");
        html += QString::fromLatin1(kMermaidRenderScript);
        html += QLatin1String("\n</script>\n");
    }
    if (needsMath) {
        html += QLatin1String("<script>\n");
        html += QString::fromLatin1(kKatexRenderScript);
        html += QLatin1String("\n</script>\n");
    }

    html += QLatin1String("</body>\n</html>\n");
    return html;
}

} // namespace hungryeditor::htmlexport
