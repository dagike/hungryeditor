// Coverage for assembling a standalone HTML export.

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "export/HtmlDocument.h"

using hungryeditor::htmlexport::build;
using hungryeditor::htmlexport::Options;

namespace {

// A 1x1 transparent PNG.
const char* const kPngBase64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4"
                               "2mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";

} // namespace

class TestHtmlDocument : public QObject
{
    Q_OBJECT

private slots:
    void wrapsTheFragmentInAFullDocument();
    void escapesTheTitle();
    void skipsKatexAndMermaidWhenUnused();
    void bundlesKatexOnlyWhenMathIsPresent();
    void bundlesMermaidOnlyWhenAFenceIsPresent();
    void inlinesALocalImage();
};

void TestHtmlDocument::wrapsTheFragmentInAFullDocument()
{
    const QString html =
        build(QStringLiteral("# Hello\n\nWorld.\n"), {QStringLiteral("Hello"), QString()});

    QVERIFY(html.startsWith(QLatin1String("<!doctype html>")));
    QVERIFY(html.contains(QLatin1String("<meta charset=\"utf-8\">")));
    QVERIFY(html.contains(QLatin1String("<title>Hello</title>")));
    QVERIFY(html.contains(QLatin1String("<h1")));
    QVERIFY(html.contains(QLatin1String("World.")));
    QVERIFY(html.contains(QLatin1String("</html>")));
}

void TestHtmlDocument::escapesTheTitle()
{
    const QString html =
        build(QStringLiteral("text"), {QStringLiteral("A <B> & \"C\""), QString()});
    QVERIFY(html.contains(QLatin1String("<title>A &lt;B&gt; &amp; &quot;C&quot;</title>")));
}

void TestHtmlDocument::skipsKatexAndMermaidWhenUnused()
{
    // The theme's base CSS mentions ".mermaid-diagram" regardless of content
    // (harmless, a few bytes) — what matters is that no library is bundled.
    const QString html =
        build(QStringLiteral("plain paragraph"), {QStringLiteral("Plain"), QString()});
    QVERIFY(!html.contains(QLatin1String("<script")));
    QVERIFY(!html.contains(QLatin1String("katex.render")));
    QVERIFY(!html.contains(QLatin1String("mermaid.render")));
}

void TestHtmlDocument::bundlesKatexOnlyWhenMathIsPresent()
{
    const QString html =
        build(QStringLiteral("Energy: $E=mc^2$\n"), {QStringLiteral("Math"), QString()});

    QVERIFY(html.contains(QLatin1String("katex.render")));
    QVERIFY(html.contains(QLatin1String("data:font/woff2;base64,")));
    QVERIFY(!html.contains(QLatin1String("url(fonts/")));
    QVERIFY(!html.contains(QLatin1String("mermaid.render")));
}

void TestHtmlDocument::bundlesMermaidOnlyWhenAFenceIsPresent()
{
    const QString html = build(QStringLiteral("```mermaid\ngraph TD;\nA-->B;\n```\n"),
                               {QStringLiteral("Diagram"), QString()});

    QVERIFY(html.contains(QLatin1String("mermaid.render")));
    QVERIFY(!html.contains(QLatin1String("katex.render")));
}

void TestHtmlDocument::inlinesALocalImage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.filePath(QStringLiteral("pic.png")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray::fromBase64(kPngBase64));
    file.close();

    Options options{QStringLiteral("Pic"), dir.path()};
    const QString html = build(QStringLiteral("![alt](pic.png)\n"), options);

    QVERIFY(html.contains(QLatin1String("data:image/png;base64,")));
    QVERIFY(!html.contains(QLatin1String("src=\"pic.png\"")));
}

QTEST_APPLESS_MAIN(TestHtmlDocument)
#include "test_html_document.moc"
