// Coverage for the Markdown -> HTML preview renderer.

#include <QtTest>

#include "markdown/Md4cRenderer.h"

using hungryeditor::Md4cRenderer;

class TestMd4cRenderer : public QObject
{
    Q_OBJECT

private slots:
    void rendersCommonMarkBlocks();
    void tagsBlocksWithTheirSourceLine();
    void fencedCodeKeepsLanguageAndEscapes();
    void escapesHtmlInProseButKeepsEntities();
    void rendersInlineSpansAndLinks();
    void emptyInputProducesEmptyFragment();
};

void TestMd4cRenderer::rendersCommonMarkBlocks()
{
    Md4cRenderer renderer;
    const QString html =
        renderer.toHtml(QStringLiteral("# Title\n\nA paragraph.\n\n- one\n- two\n"));

    QVERIFY(html.contains(QStringLiteral(">Title</h1>")));
    QVERIFY(html.contains(QStringLiteral(">A paragraph.</p>")));
    QVERIFY(html.contains(QStringLiteral("<ul data-src-line=\"4\">")));
    QVERIFY(html.contains(QStringLiteral(">one</li>")));
    QVERIFY(html.contains(QStringLiteral(">two</li>")));
}

void TestMd4cRenderer::tagsBlocksWithTheirSourceLine()
{
    Md4cRenderer renderer;
    //             0          1  2            3  4
    const QString src = QStringLiteral("# Heading\n\nfirst para\n\nsecond para\n");
    const QString html = renderer.toHtml(src);

    QVERIFY(html.contains(QStringLiteral("<h1 data-src-line=\"0\">")));
    QVERIFY(html.contains(QStringLiteral("<p data-src-line=\"2\">first para</p>")));
    QVERIFY(html.contains(QStringLiteral("<p data-src-line=\"4\">second para</p>")));
}

void TestMd4cRenderer::fencedCodeKeepsLanguageAndEscapes()
{
    Md4cRenderer renderer;
    const QString html =
        renderer.toHtml(QStringLiteral("```rust\nfn main() { let x = a < b && c > d; }\n```\n"));

    QVERIFY(
        html.contains(QStringLiteral("<pre data-src-line=\"0\"><code class=\"language-rust\">")));
    QVERIFY(html.contains(QStringLiteral("a &lt; b &amp;&amp; c &gt; d")));
    QVERIFY(html.contains(QStringLiteral("</code></pre>")));
}

void TestMd4cRenderer::escapesHtmlInProseButKeepsEntities()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("5 &lt; 10 and a<b tag &amp; more\n"));

    // The literal `<b` is escaped; the source entities pass straight through.
    QVERIFY(html.contains(QStringLiteral("a&lt;b tag")));
    QVERIFY(html.contains(QStringLiteral("5 &lt; 10")));
    QVERIFY(html.contains(QStringLiteral("&amp; more")));
}

void TestMd4cRenderer::rendersInlineSpansAndLinks()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(
        QStringLiteral("Text with *soft*, **loud**, `code` and a [link](https://example.com).\n"));

    QVERIFY(html.contains(QStringLiteral("<em>soft</em>")));
    QVERIFY(html.contains(QStringLiteral("<strong>loud</strong>")));
    QVERIFY(html.contains(QStringLiteral("<code>code</code>")));
    QVERIFY(html.contains(QStringLiteral("<a href=\"https://example.com\">link</a>")));
}

void TestMd4cRenderer::emptyInputProducesEmptyFragment()
{
    Md4cRenderer renderer;
    QVERIFY(renderer.toHtml(QString()).trimmed().isEmpty());
}

QTEST_APPLESS_MAIN(TestMd4cRenderer)
#include "test_md4c_renderer.moc"
