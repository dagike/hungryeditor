// Coverage for the synchronous fenced-code highlighter.

#include <string_view>

#include <QtTest>

#include "highlight/CaptureStyles.h"
#include "highlight/CodeHighlighter.h"

using hungryeditor::CodeToken;
using hungryeditor::highlightCode;

class TestCodeHighlighter : public QObject
{
    Q_OBJECT

private slots:
    void highlightsAKnownLanguage();
    void tokensTileTheWholeInput();
    void unknownOrEmptyLanguageYieldsNothing();
};

void TestCodeHighlighter::highlightsAKnownLanguage()
{
    const std::vector<CodeToken> tokens = highlightCode("rust", "fn main() { let x = 1; }");
    QVERIFY(!tokens.empty());

    bool fnIsKeyword = false;
    for (const CodeToken& token : tokens) {
        if (token.start == 0 && token.length == 2 && token.style == hungryeditor::StyleKeyword) {
            fnIsKeyword = true;
        }
    }
    QVERIFY(fnIsKeyword);
}

void TestCodeHighlighter::tokensTileTheWholeInput()
{
    const std::string_view code = "fn demo() -> i32 { 0 }";
    const std::vector<CodeToken> tokens = highlightCode("rs", code); // alias for rust
    QVERIFY(!tokens.empty());

    int covered = 0;
    for (const CodeToken& token : tokens) {
        QCOMPARE(token.start, covered);
        covered += token.length;
    }
    QCOMPARE(covered, static_cast<int>(code.size()));
}

void TestCodeHighlighter::unknownOrEmptyLanguageYieldsNothing()
{
    QVERIFY(highlightCode("nonesuch", "some code").empty());
    QVERIFY(highlightCode("", "some code").empty());
    QVERIFY(highlightCode("rust", "").empty());
}

QTEST_APPLESS_MAIN(TestCodeHighlighter)
#include "test_code_highlighter.moc"
