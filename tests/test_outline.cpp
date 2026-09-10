// Coverage for the Markdown heading extractor.

#include <QtTest>

#include "markdown/Outline.h"

using hungryeditor::outline::Heading;
using hungryeditor::outline::parse;

class TestOutline : public QObject
{
    Q_OBJECT

private slots:
    void extractsAtxHeadingsWithLevels();
    void stripsClosingHashRuns();
    void readsSetextHeadings();
    void ignoresHeadingsInsideFencedCode();
    void skipsAFrontMatterBlock();
};

void TestOutline::extractsAtxHeadingsWithLevels()
{
    const QVector<Heading> headings =
        parse(QStringLiteral("# Title\n\nintro\n\n## Section\n\ntext\n### Sub\n"));
    QCOMPARE(headings.size(), 3);
    QCOMPARE(headings.at(0).level, 1);
    QCOMPARE(headings.at(0).text, QStringLiteral("Title"));
    QCOMPARE(headings.at(0).line, 0);
    QCOMPARE(headings.at(1).level, 2);
    QCOMPARE(headings.at(1).text, QStringLiteral("Section"));
    QCOMPARE(headings.at(1).line, 4);
    QCOMPARE(headings.at(2).level, 3);
    QCOMPARE(headings.at(2).line, 7);
}

void TestOutline::stripsClosingHashRuns()
{
    const QVector<Heading> headings = parse(QStringLiteral("## Heading ##\n\n### Done ###\n"));
    QCOMPARE(headings.size(), 2);
    QCOMPARE(headings.at(0).text, QStringLiteral("Heading"));
    QCOMPARE(headings.at(1).text, QStringLiteral("Done"));

    // A `#` not separated by a space is literal.
    const QVector<Heading> literal = parse(QStringLiteral("# C#\n"));
    QCOMPARE(literal.size(), 1);
    QCOMPARE(literal.at(0).text, QStringLiteral("C#"));
}

void TestOutline::readsSetextHeadings()
{
    const QVector<Heading> headings =
        parse(QStringLiteral("Big Title\n=========\n\nSmaller\n-------\n"));
    QCOMPARE(headings.size(), 2);
    QCOMPARE(headings.at(0).level, 1);
    QCOMPARE(headings.at(0).text, QStringLiteral("Big Title"));
    QCOMPARE(headings.at(0).line, 0);
    QCOMPARE(headings.at(1).level, 2);
    QCOMPARE(headings.at(1).text, QStringLiteral("Smaller"));
    QCOMPARE(headings.at(1).line, 3);
}

void TestOutline::ignoresHeadingsInsideFencedCode()
{
    const QVector<Heading> headings =
        parse(QStringLiteral("# Real\n\n```\n# fake heading\n```\n\n## Also Real\n"));
    QCOMPARE(headings.size(), 2);
    QCOMPARE(headings.at(0).text, QStringLiteral("Real"));
    QCOMPARE(headings.at(1).text, QStringLiteral("Also Real"));
    QCOMPARE(headings.at(1).line, 6);
}

void TestOutline::skipsAFrontMatterBlock()
{
    const QVector<Heading> headings =
        parse(QStringLiteral("---\ntitle: X\n# not a heading\n---\n\n# Actual\n"));
    QCOMPARE(headings.size(), 1);
    QCOMPARE(headings.at(0).text, QStringLiteral("Actual"));
    QCOMPARE(headings.at(0).line, 5);
}

QTEST_APPLESS_MAIN(TestOutline)
#include "test_outline.moc"
