// Coverage for the workspace folder sidebar.

#include <QAction>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QTreeWidget>
#include <QtTest>

#include "ui/FileTreePanel.h"

using hungryeditor::FileTreePanel;

namespace {

QStringList sampleFiles()
{
    return {
        QStringLiteral("/ws/readme.md"),
        QStringLiteral("/ws/docs/intro.md"),
        QStringLiteral("/ws/docs/guide.md"),
        QStringLiteral("/ws/docs/api/reference.md"),
    };
}

QTreeWidgetItem* childNamed(QTreeWidgetItem* parent, const QString& name)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        if (parent->child(i)->text(0) == name) {
            return parent->child(i);
        }
    }
    return nullptr;
}

} // namespace

class TestFileTreePanel : public QObject
{
    Q_OBJECT

private slots:
    void buildsANestedTreeWithDirectoriesFirst();
    void ignoresPathsOutsideTheRoot();
    void activatingAFileEmitsItsAbsolutePath();
    void activatingADirectoryEmitsNothing();
    void filteringHidesNonMatchingRows();
    void clearingTheFilterRestoresEveryRow();
    void anEmptyRootShowsThePlaceholder();
    void changingTheRootClearsTheTree();
    void filterTextRoundTrips();
    void restoringExpandedDirsCollapsesTheRest();
    void expandedDirectoriesReportsThemBack();
    void contextMenuOnAFileTargetsItAndItsParent();
    void contextMenuOnEmptySpaceTargetsTheRoot();
    void noContextMenuWithoutAFolder();
};

namespace {

QAction* actionNamed(QMenu* menu, const QString& text)
{
    for (QAction* action : menu->actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

void TestFileTreePanel::buildsANestedTreeWithDirectoriesFirst()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());

    auto* tree = panel.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);
    QVERIFY(!tree->isHidden());
    QCOMPARE(tree->topLevelItemCount(), 2);

    // "docs" (directory) sorts before "readme.md" (file).
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("docs"));
    QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("readme.md"));

    QTreeWidgetItem* docs = tree->topLevelItem(0);
    QCOMPARE(docs->childCount(), 3); // api/, guide.md, intro.md
    QCOMPARE(docs->child(0)->text(0), QStringLiteral("api"));
    QVERIFY(childNamed(docs, QStringLiteral("guide.md")) != nullptr);

    QTreeWidgetItem* api = docs->child(0);
    QCOMPARE(api->childCount(), 1);
    QCOMPARE(api->child(0)->text(0), QStringLiteral("reference.md"));
}

void TestFileTreePanel::ignoresPathsOutsideTheRoot()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles({QStringLiteral("/ws/keep.md"), QStringLiteral("/elsewhere/drop.md")});

    auto* tree = panel.findChild<QTreeWidget*>();
    QCOMPARE(tree->topLevelItemCount(), 1);
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("keep.md"));
}

void TestFileTreePanel::activatingAFileEmitsItsAbsolutePath()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());

    auto* tree = panel.findChild<QTreeWidget*>();
    QSignalSpy activated(&panel, &FileTreePanel::fileActivated);

    QTreeWidgetItem* docs = tree->topLevelItem(0);
    QTreeWidgetItem* intro = childNamed(docs, QStringLiteral("intro.md"));
    QVERIFY(intro != nullptr);
    QMetaObject::invokeMethod(tree, "itemActivated", Q_ARG(QTreeWidgetItem*, intro), Q_ARG(int, 0));

    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().first().toString(), QStringLiteral("/ws/docs/intro.md"));
}

void TestFileTreePanel::activatingADirectoryEmitsNothing()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());

    auto* tree = panel.findChild<QTreeWidget*>();
    QSignalSpy activated(&panel, &FileTreePanel::fileActivated);

    QMetaObject::invokeMethod(tree, "itemActivated", Q_ARG(QTreeWidgetItem*, tree->topLevelItem(0)),
                              Q_ARG(int, 0)); // "docs"
    QCOMPARE(activated.count(), 0);
}

void TestFileTreePanel::filteringHidesNonMatchingRows()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());

    auto* filter = panel.findChild<QLineEdit*>();
    auto* tree = panel.findChild<QTreeWidget*>();
    filter->setText(QStringLiteral("guide"));

    QTreeWidgetItem* docs = tree->topLevelItem(0);
    QVERIFY(!docs->isHidden());
    QVERIFY(!childNamed(docs, QStringLiteral("guide.md"))->isHidden());
    QVERIFY(childNamed(docs, QStringLiteral("intro.md"))->isHidden());
    QVERIFY(docs->child(0)->isHidden());        // "api" has no matching descendant
    QVERIFY(tree->topLevelItem(1)->isHidden()); // readme.md
}

void TestFileTreePanel::clearingTheFilterRestoresEveryRow()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());

    auto* filter = panel.findChild<QLineEdit*>();
    auto* tree = panel.findChild<QTreeWidget*>();
    filter->setText(QStringLiteral("guide"));
    filter->clear();

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QVERIFY(!tree->topLevelItem(i)->isHidden());
    }
    QTreeWidgetItem* docs = tree->topLevelItem(0);
    QVERIFY(!childNamed(docs, QStringLiteral("intro.md"))->isHidden());
}

void TestFileTreePanel::anEmptyRootShowsThePlaceholder()
{
    FileTreePanel panel;

    auto* tree = panel.findChild<QTreeWidget*>();
    auto* placeholder = panel.findChild<QLabel*>();
    QVERIFY(tree->isHidden());
    QVERIFY(!placeholder->isHidden());
}

void TestFileTreePanel::changingTheRootClearsTheTree()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());
    QVERIFY(panel.findChild<QTreeWidget*>()->topLevelItemCount() > 0);

    panel.setRoot(QStringLiteral("/other"));
    QCOMPARE(panel.findChild<QTreeWidget*>()->topLevelItemCount(), 0);
    QVERIFY(!panel.findChild<QLabel*>()->isHidden());
}

void TestFileTreePanel::filterTextRoundTrips()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());
    panel.applyState(QStringLiteral("guide"), {}, false);

    QCOMPARE(panel.filterText(), QStringLiteral("guide"));
    auto* tree = panel.findChild<QTreeWidget*>();
    QVERIFY(tree->topLevelItem(1)->isHidden()); // readme.md filtered out
}

void TestFileTreePanel::restoringExpandedDirsCollapsesTheRest()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());
    panel.applyState(QString(), {QStringLiteral("docs")}, true);

    auto* tree = panel.findChild<QTreeWidget*>();
    QTreeWidgetItem* docs = tree->topLevelItem(0);
    QVERIFY(docs->isExpanded());
    QVERIFY(!docs->child(0)->isExpanded()); // "docs/api" left collapsed
}

void TestFileTreePanel::expandedDirectoriesReportsThemBack()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles()); // expand-all default

    QCOMPARE(panel.expandedDirectories(),
             (QStringList{QStringLiteral("docs"), QStringLiteral("docs/api")}));

    panel.applyState(QString(), {QStringLiteral("docs")}, true);
    QCOMPARE(panel.expandedDirectories(), QStringList{QStringLiteral("docs")});
}

void TestFileTreePanel::contextMenuOnAFileTargetsItAndItsParent()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());
    auto* tree = panel.findChild<QTreeWidget*>();
    QTreeWidgetItem* guide = childNamed(tree->topLevelItem(0), QStringLiteral("guide.md"));
    QVERIFY(guide != nullptr);

    QMenu* menu = panel.contextMenuFor(guide);
    QVERIFY(menu != nullptr);

    QSignalSpy newFile(&panel, &FileTreePanel::createFileRequested);
    QSignalSpy renamed(&panel, &FileTreePanel::renameRequested);

    actionNamed(menu, QStringLiteral("New File…"))->trigger();
    QCOMPARE(newFile.first().first().toString(), QStringLiteral("/ws/docs")); // the file's parent

    actionNamed(menu, QStringLiteral("Rename…"))->trigger();
    QCOMPARE(renamed.first().at(0).toString(), QStringLiteral("/ws/docs/guide.md"));
    QCOMPARE(renamed.first().at(1).toBool(), false); // not a directory

    delete menu;
}

void TestFileTreePanel::contextMenuOnEmptySpaceTargetsTheRoot()
{
    FileTreePanel panel;
    panel.setRoot(QStringLiteral("/ws"));
    panel.setFiles(sampleFiles());

    QMenu* menu = panel.contextMenuFor(nullptr);
    QVERIFY(menu != nullptr);
    QVERIFY(actionNamed(menu, QStringLiteral("Rename…")) == nullptr); // nothing to rename

    QSignalSpy newFolder(&panel, &FileTreePanel::createFolderRequested);
    actionNamed(menu, QStringLiteral("New Folder…"))->trigger();
    QCOMPARE(newFolder.first().first().toString(), QStringLiteral("/ws"));

    delete menu;
}

void TestFileTreePanel::noContextMenuWithoutAFolder()
{
    FileTreePanel panel;
    QVERIFY(panel.contextMenuFor(nullptr) == nullptr);
}

QTEST_MAIN(TestFileTreePanel)
#include "test_file_tree_panel.moc"
