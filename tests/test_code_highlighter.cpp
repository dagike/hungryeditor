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
    void predicatesNarrowConstantAndConstructorCaptures();
    void highlightsSql();
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

void TestCodeHighlighter::predicatesNarrowConstantAndConstructorCaptures()
{
    // python's highlights.scm defines, in order: an unconditional @variable
    // on every identifier, then a #match?-gated @constructor ("^[A-Z]"), then
    // a #match?-gated @constant ("^[A-Z][A-Z_]*$"). Byte-painting has later
    // patterns win, so before predicates were evaluated every identifier —
    // regardless of case — ended up styled as the last rule, @constant. With
    // predicates evaluated, only the rule(s) whose #match? actually holds
    // for a given identifier apply.
    const std::string_view code = "demo = 1\nWidget = 2\nMAX_SIZE = 3\n";
    const std::vector<CodeToken> tokens = highlightCode("python", code);
    QVERIFY(!tokens.empty());

    const auto styleAt = [&](int start, int length) -> int {
        for (const CodeToken& token : tokens) {
            if (token.start == start && token.length == length) {
                return token.style;
            }
        }
        return -1;
    };

    QCOMPARE(styleAt(0, 4), static_cast<int>(hungryeditor::StyleVariable));  // demo
    QCOMPARE(styleAt(9, 6), static_cast<int>(hungryeditor::StyleType));      // Widget: constructor
    QCOMPARE(styleAt(20, 8), static_cast<int>(hungryeditor::StyleConstant)); // MAX_SIZE: constant
}

void TestCodeHighlighter::highlightsSql()
{
    const std::string_view code = "SELECT id FROM users;";
    const std::vector<CodeToken> tokens = highlightCode("sql", code);
    QVERIFY(!tokens.empty());

    const auto styleAt = [&](int start, int length) -> int {
        for (const CodeToken& token : tokens) {
            if (token.start == start && token.length == length) {
                return token.style;
            }
        }
        return -1;
    };

    QCOMPARE(styleAt(0, 6), static_cast<int>(hungryeditor::StyleKeyword));  // SELECT
    QCOMPARE(styleAt(10, 4), static_cast<int>(hungryeditor::StyleKeyword)); // FROM

    // Aliases resolve to the same grammar.
    QVERIFY(!highlightCode("postgresql", code).empty());
}

QTEST_APPLESS_MAIN(TestCodeHighlighter)
#include "test_code_highlighter.moc"
