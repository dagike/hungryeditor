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
    void fencedCodeIsSyntaxHighlighted();
    void unknownFenceLanguageStaysPlain();
    void escapesHtmlInProseButKeepsEntities();
    void rendersInlineSpansAndLinks();
    void rendersGfmTables();
    void rendersTaskListItems();
    void rendersBareUrlAutolinks();
    void rendersStrikethrough();
    void rendersFootnotes();
    void rendersFrontMatterAsACard();
    void plainThematicBreakStaysAnHr();
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
    const QString html = renderer.toHtml(QStringLiteral("```rust\nlet s = a < b && c > d;\n```\n"));

    QVERIFY(
        html.contains(QStringLiteral("<pre data-src-line=\"0\"><code class=\"language-rust\">")));
    QVERIFY(html.contains(QStringLiteral("</code></pre>")));
    // The angle brackets and ampersands are still escaped, even with token
    // spans woven through the code.
    QVERIFY(html.contains(QStringLiteral("&lt;")));
    QVERIFY(html.contains(QStringLiteral("&gt;")));
    QVERIFY(html.contains(QStringLiteral("&amp;&amp;")));
    QVERIFY(!html.contains(QStringLiteral("<b ")));
}

void TestMd4cRenderer::fencedCodeIsSyntaxHighlighted()
{
    Md4cRenderer renderer;
    const QString html =
        renderer.toHtml(QStringLiteral("```rust\nfn demo() { let x = 1; }\n```\n"));

    QVERIFY(html.contains(QStringLiteral("<span class=\"tok-keyword\">fn</span>")));
    QVERIFY(html.contains(QStringLiteral("<span class=\"tok-keyword\">let</span>")));
    QVERIFY(html.contains(QStringLiteral("<span class=\"tok-function\">demo</span>")));
}

void TestMd4cRenderer::unknownFenceLanguageStaysPlain()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("```nonesuch\nkeep me < plain\n```\n"));

    QVERIFY(html.contains(QStringLiteral(">keep me &lt; plain\n</code></pre>")));
    QVERIFY(!html.contains(QStringLiteral("tok-")));
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

void TestMd4cRenderer::rendersGfmTables()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("| Name | Qty |\n"
                                                        "|:-----|----:|\n"
                                                        "| Pear | 3   |\n"));

    QVERIFY(html.contains(QStringLiteral("<table data-src-line=\"0\">")));
    QVERIFY(html.contains(QStringLiteral("<thead>")));
    QVERIFY(html.contains(QStringLiteral("<th style=\"text-align:left\">Name</th>")));
    QVERIFY(html.contains(QStringLiteral("<th style=\"text-align:right\">Qty</th>")));
    QVERIFY(html.contains(QStringLiteral("<tbody>")));
    QVERIFY(html.contains(QStringLiteral("<td style=\"text-align:left\">Pear</td>")));
    QVERIFY(html.contains(QStringLiteral("<td style=\"text-align:right\">3</td>")));
}

void TestMd4cRenderer::rendersTaskListItems()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("- [ ] todo\n- [x] done\n"));

    QVERIFY(html.contains(QStringLiteral("<li class=\"task-list-item\" data-src-line=\"0\">")));
    QVERIFY(
        html.contains(QStringLiteral("<input type=\"checkbox\" class=\"task-checkbox\"> todo")));
    QVERIFY(html.contains(
        QStringLiteral("<input type=\"checkbox\" class=\"task-checkbox\" checked> done")));
    QVERIFY(!html.contains(QStringLiteral("disabled")));
}

void TestMd4cRenderer::rendersBareUrlAutolinks()
{
    Md4cRenderer renderer;
    const QString html =
        renderer.toHtml(QStringLiteral("See https://example.com/docs for details.\n"));

    QVERIFY(html.contains(
        QStringLiteral("<a href=\"https://example.com/docs\">https://example.com/docs</a>")));
}

void TestMd4cRenderer::rendersStrikethrough()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("This is ~~gone~~ now.\n"));

    QVERIFY(html.contains(QStringLiteral("<del>gone</del>")));
}

void TestMd4cRenderer::rendersFootnotes()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("Text with a note.[^note]\n"
                                                        "\n"
                                                        "[^note]: The *note* body.\n"));

    QVERIFY(html.contains(QStringLiteral(
        "<sup class=\"fn-ref\"><a href=\"#fn-note\" id=\"fnref-note\">1</a></sup>")));
    QVERIFY(html.contains(QStringLiteral("<section class=\"footnotes\"")));
    QVERIFY(html.contains(QStringLiteral("<li id=\"fn-note\">")));
    QVERIFY(html.contains(QStringLiteral("The <em>note</em> body.")));
    QVERIFY(html.contains(QStringLiteral("<a href=\"#fnref-note\" class=\"fn-backref\">")));
    QVERIFY(!html.contains(QStringLiteral("[^note]:")));
}

void TestMd4cRenderer::rendersFrontMatterAsACard()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("---\n"
                                                        "title: *Draft* Notes\n"
                                                        "tags: [a, b]\n"
                                                        "---\n"
                                                        "\n"
                                                        "# Body\n"));

    QVERIFY(html.contains(QStringLiteral("<div class=\"front-matter-card\" data-src-line=\"0\">")));
    QVERIFY(html.contains(QStringLiteral("<dt>title</dt><dd><em>Draft</em> Notes</dd>")));
    QVERIFY(html.contains(QStringLiteral("<dt>tags</dt><dd>a, b</dd>")));
    QVERIFY(html.contains(QStringLiteral(">Body</h1>")));
    QVERIFY(!html.contains(QStringLiteral("title: *Draft*")));
}

void TestMd4cRenderer::plainThematicBreakStaysAnHr()
{
    Md4cRenderer renderer;
    const QString html = renderer.toHtml(QStringLiteral("A paragraph.\n\n---\n\nAnother.\n"));

    QVERIFY(html.contains(QStringLiteral("<hr")));
    QVERIFY(!html.contains(QStringLiteral("front-matter-card")));
}

void TestMd4cRenderer::emptyInputProducesEmptyFragment()
{
    Md4cRenderer renderer;
    QVERIFY(renderer.toHtml(QString()).trimmed().isEmpty());
}

QTEST_APPLESS_MAIN(TestMd4cRenderer)
#include "test_md4c_renderer.moc"
