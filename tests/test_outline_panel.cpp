// Coverage for the heading outline side panel.

#include <QLabel>
#include <QSignalSpy>
#include <QTreeWidget>
#include <QtTest>

#include "ui/OutlinePanel.h"

using hungryeditor::OutlinePanel;
using hungryeditor::outline::Heading;

namespace {

QVector<Heading> sampleHeadings()
{
    return {
        {1, QStringLiteral("Alpha"), 0},
        {2, QStringLiteral("Beta"), 2},
        {3, QStringLiteral("Gamma"), 4},
        {1, QStringLiteral("Delta"), 10},
    };
}

} // namespace

class TestOutlinePanel : public QObject
{
    Q_OBJECT

private slots:
    void buildsANestedTree();
    void activatingAnItemEmitsItsSourceLine();
    void highlightLineSelectsTheEnclosingHeading();
    void anEmptyDocumentShowsThePlaceholder();
};

void TestOutlinePanel::buildsANestedTree()
{
    OutlinePanel panel;
    panel.setHeadings(sampleHeadings());

    auto* tree = panel.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);
    QVERIFY(!tree->isHidden());
    QCOMPARE(tree->topLevelItemCount(), 2); // Alpha, Delta

    QTreeWidgetItem* alpha = tree->topLevelItem(0);
    QCOMPARE(alpha->text(0), QStringLiteral("Alpha"));
    QCOMPARE(alpha->childCount(), 1); // Beta
    QCOMPARE(alpha->child(0)->text(0), QStringLiteral("Beta"));
    QCOMPARE(alpha->child(0)->childCount(), 1); // Gamma
    QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("Delta"));
}

void TestOutlinePanel::activatingAnItemEmitsItsSourceLine()
{
    OutlinePanel panel;
    panel.setHeadings(sampleHeadings());
    auto* tree = panel.findChild<QTreeWidget*>();

    QSignalSpy activated(&panel, &OutlinePanel::headingActivated);
    QMetaObject::invokeMethod(tree, "itemClicked", Q_ARG(QTreeWidgetItem*, tree->topLevelItem(1)),
                              Q_ARG(int, 0));
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().at(0).toInt(), 10);
}

void TestOutlinePanel::highlightLineSelectsTheEnclosingHeading()
{
    OutlinePanel panel;
    panel.setHeadings(sampleHeadings());
    auto* tree = panel.findChild<QTreeWidget*>();

    QSignalSpy activated(&panel, &OutlinePanel::headingActivated);

    panel.highlightLine(5);
    QVERIFY(tree->currentItem() != nullptr);
    QCOMPARE(tree->currentItem()->text(0), QStringLiteral("Gamma"));

    panel.highlightLine(20);
    QCOMPARE(tree->currentItem()->text(0), QStringLiteral("Delta"));

    panel.highlightLine(0);
    QCOMPARE(tree->currentItem()->text(0), QStringLiteral("Alpha"));

    QCOMPARE(activated.count(), 0); // highlighting never activates
}

void TestOutlinePanel::anEmptyDocumentShowsThePlaceholder()
{
    OutlinePanel panel;
    panel.setHeadings({});

    auto* tree = panel.findChild<QTreeWidget*>();
    auto* placeholder = panel.findChild<QLabel*>();
    QVERIFY(tree->isHidden());
    QVERIFY(!placeholder->isHidden());
}

QTEST_MAIN(TestOutlinePanel)
#include "test_outline_panel.moc"
