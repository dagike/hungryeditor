// Coverage for the command palette widget.

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalSpy>
#include <QtTest>

#include "ui/CommandPalette.h"

using hungryeditor::CommandPalette;

class TestCommandPalette : public QObject
{
    Q_OBJECT

private slots:
    void filtersFuzzilyAndAcceptsWithEnter();
};

void TestCommandPalette::filtersFuzzilyAndAcceptsWithEnter()
{
    CommandPalette palette;
    palette.setCommands(
        {{QStringLiteral("action.open"), QStringLiteral("Open File"), QStringLiteral("Ctrl+O")},
         {QStringLiteral("action.save"), QStringLiteral("Save"), QStringLiteral("Ctrl+S")},
         {QStringLiteral("action.close"), QStringLiteral("Close Tab"), QStringLiteral("Ctrl+W")}});
    palette.open();

    auto* query = palette.findChild<QLineEdit*>();
    auto* list = palette.findChild<QListWidget*>();
    QVERIFY(query != nullptr);
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 3); // an empty query lists everything

    query->setText(QStringLiteral("sv")); // fuzzy match for "Save"
    QCOMPARE(list->count(), 1);
    QVERIFY(list->item(0)->text().startsWith(QStringLiteral("Save")));

    QSignalSpy chosen(&palette, &CommandPalette::commandChosen);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(query, &enter);

    QCOMPARE(chosen.count(), 1);
    QCOMPARE(chosen.first().at(0).toString(), QStringLiteral("action.save"));
    QVERIFY(palette.isHidden());
}

QTEST_MAIN(TestCommandPalette)
#include "test_command_palette.moc"
