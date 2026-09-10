// Coverage for the YAML front-matter parser.

#include <QtTest>

#include "markdown/FrontMatter.h"

using hungryeditor::frontmatter::FrontMatter;
using hungryeditor::frontmatter::parse;

class TestFrontMatter : public QObject
{
    Q_OBJECT

private slots:
    void detectsABlockAndItsBounds();
    void requiresADashFirstLine();
    void midDocumentDashesAreNotFrontMatter();
    void parsesScalarFields();
    void joinsBlockAndFlowSequences();
    void trimsQuotesAndSkipsComments();
};

void TestFrontMatter::detectsABlockAndItsBounds()
{
    const FrontMatter fm = parse(QStringLiteral("---\ntitle: Hello\n---\n\nBody\n"));
    QVERIFY(fm.present);
    QCOMPARE(fm.firstLine, 0);
    QCOMPARE(fm.lastLine, 2);
    QCOMPARE(fm.fields.size(), 1);
    QCOMPARE(fm.fields.first().first, QStringLiteral("title"));
    QCOMPARE(fm.fields.first().second, QStringLiteral("Hello"));
}

void TestFrontMatter::requiresADashFirstLine()
{
    QVERIFY(!parse(QStringLiteral("# Heading\n\ntext\n")).present);
    QVERIFY(!parse(QStringLiteral("\n---\ntitle: x\n---\n")).present);
    QVERIFY(!parse(QStringLiteral("---\ntitle: unterminated\n")).present);
}

void TestFrontMatter::midDocumentDashesAreNotFrontMatter()
{
    QVERIFY(!parse(QStringLiteral("Intro paragraph.\n\n---\n\nMore.\n")).present);
}

void TestFrontMatter::parsesScalarFields()
{
    const FrontMatter fm = parse(
        QStringLiteral("---\ntitle: My Doc\ndate: 2026-09-09\nurl: https://example.com/x\n---\n"));
    QCOMPARE(fm.fields.size(), 3);
    QCOMPARE(fm.fields.at(1).first, QStringLiteral("date"));
    QCOMPARE(fm.fields.at(1).second, QStringLiteral("2026-09-09"));
    QCOMPARE(fm.fields.at(2).second, QStringLiteral("https://example.com/x"));
}

void TestFrontMatter::joinsBlockAndFlowSequences()
{
    const FrontMatter flow = parse(QStringLiteral("---\ntags: [a, b, c]\n---\n"));
    QCOMPARE(flow.fields.first().second, QStringLiteral("a, b, c"));

    const FrontMatter block = parse(QStringLiteral("---\ntags:\n  - one\n  - two\n---\n"));
    QCOMPARE(block.fields.first().first, QStringLiteral("tags"));
    QCOMPARE(block.fields.first().second, QStringLiteral("one, two"));
}

void TestFrontMatter::trimsQuotesAndSkipsComments()
{
    const FrontMatter fm = parse(
        QStringLiteral("---\n# a comment\ntitle: \"Quoted Title\"\nsummary: 'single'\n---\n"));
    QCOMPARE(fm.fields.size(), 2);
    QCOMPARE(fm.fields.at(0).second, QStringLiteral("Quoted Title"));
    QCOMPARE(fm.fields.at(1).second, QStringLiteral("single"));
}

QTEST_APPLESS_MAIN(TestFrontMatter)
#include "test_front_matter.moc"
