// Coverage for the GFM pipe-table helpers.

#include <QtTest>

#include "editor/MarkdownTable.h"

using namespace hungryeditor::mdtable;

class TestMarkdownTable : public QObject
{
    Q_OBJECT

private slots:
    void recognisesDelimiterRows();
    void findsTheRegionOnlyWithADelimiter();
    void parsesColumnAlignments();
    void splitsCellsHonouringEscapes();
    void rendersAlignedColumns();
};

void TestMarkdownTable::recognisesDelimiterRows()
{
    QVERIFY(isDelimiterRow(QStringLiteral("|---|---|").toStdString()));
    QVERIFY(isDelimiterRow(QStringLiteral(" :--- | ---: | :--: ").toStdString()));
    QVERIFY(!isDelimiterRow(QStringLiteral("| a | b |").toStdString()));
    QVERIFY(!isDelimiterRow(QStringLiteral("| -- | x |").toStdString()));
}

void TestMarkdownTable::findsTheRegionOnlyWithADelimiter()
{
    const std::vector<std::string> withTable = {
        "intro", "| a | b |", "|---|---|", "| 1 | 2 |", "outro",
    };
    const TableRegion region = findTableRegion(withTable, 3);
    QVERIFY(region.valid);
    QCOMPARE(region.firstLine, 1);
    QCOMPARE(region.lastLine, 3);

    const std::vector<std::string> noDelimiter = {"| a | b |", "| 1 | 2 |"};
    QVERIFY(!findTableRegion(noDelimiter, 0).valid);
}

void TestMarkdownTable::parsesColumnAlignments()
{
    const auto aligns = parseAlignments(QStringLiteral("|:--|--:|:-:|---|").toStdString());
    QCOMPARE(aligns.size(), std::size_t(4));
    QVERIFY(aligns[0] == ColumnAlign::Left);
    QVERIFY(aligns[1] == ColumnAlign::Right);
    QVERIFY(aligns[2] == ColumnAlign::Center);
    QVERIFY(aligns[3] == ColumnAlign::None);
}

void TestMarkdownTable::splitsCellsHonouringEscapes()
{
    const auto cells = splitCells(QStringLiteral("| a \\| b | c |").toStdString());
    QCOMPARE(cells.size(), std::size_t(2));
    QCOMPARE(QString::fromStdString(cells[0]), QStringLiteral("a | b"));
    QCOMPARE(QString::fromStdString(cells[1]), QStringLiteral("c"));
}

void TestMarkdownTable::rendersAlignedColumns()
{
    const std::vector<std::vector<std::string>> rows = {
        {"Name", "Qty"},
        {"Widget", "3"},
        {"Gadget", "12"},
    };
    const std::vector<ColumnAlign> aligns = {ColumnAlign::None, ColumnAlign::Right};

    const QString out = QString::fromStdString(renderAligned(rows, aligns));
    const QStringList lines = out.split(QLatin1Char('\n'));

    QCOMPARE(lines.size(), 4);
    QCOMPARE(lines[0], QStringLiteral("| Name   | Qty |"));
    QCOMPARE(lines[1], QStringLiteral("| ------ | --: |"));
    QCOMPARE(lines[2], QStringLiteral("| Widget |   3 |"));
    QCOMPARE(lines[3], QStringLiteral("| Gadget |  12 |"));
}

QTEST_MAIN(TestMarkdownTable)
#include "test_markdown_table.moc"
