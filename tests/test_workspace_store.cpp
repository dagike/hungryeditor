// Coverage for per-folder workspace view state.

#include <QTemporaryDir>
#include <QtTest>

#include "workspace/WorkspaceStore.h"

using hungryeditor::WorkspaceState;
using hungryeditor::WorkspaceStore;

class TestWorkspaceStore : public QObject
{
    Q_OBJECT

private slots:
    void unknownFolderReturnsADefault();
    void roundTripsFilterAndExpandedDirs();
    void keepsFoldersIndependent();
    void savingAgainReplacesTheEntry();
    void anEmptyExpandedListStillCountsAsSaved();
};

void TestWorkspaceStore::unknownFolderReturnsADefault()
{
    QTemporaryDir dir;
    WorkspaceStore store(dir.filePath(QStringLiteral("workspaces.json")));

    const WorkspaceState state = store.load(QStringLiteral("/no/such/folder"));
    QVERIFY(state.filter.isEmpty());
    QVERIFY(state.expandedDirs.isEmpty());
    QVERIFY(!state.hasExpandedList);
}

void TestWorkspaceStore::roundTripsFilterAndExpandedDirs()
{
    QTemporaryDir dir;
    WorkspaceStore store(dir.filePath(QStringLiteral("workspaces.json")));

    WorkspaceState state;
    state.filter = QStringLiteral("readme");
    state.expandedDirs = {QStringLiteral("src"), QStringLiteral("src/ui")};
    state.hasExpandedList = true;
    QVERIFY(store.save(QStringLiteral("/home/me/proj"), state));

    const WorkspaceState back = store.load(QStringLiteral("/home/me/proj"));
    QCOMPARE(back.filter, QStringLiteral("readme"));
    QCOMPARE(back.expandedDirs, (QStringList{QStringLiteral("src"), QStringLiteral("src/ui")}));
    QVERIFY(back.hasExpandedList);
}

void TestWorkspaceStore::keepsFoldersIndependent()
{
    QTemporaryDir dir;
    WorkspaceStore store(dir.filePath(QStringLiteral("workspaces.json")));

    WorkspaceState a;
    a.filter = QStringLiteral("alpha");
    a.hasExpandedList = true;
    WorkspaceState b;
    b.filter = QStringLiteral("beta");
    b.hasExpandedList = true;
    QVERIFY(store.save(QStringLiteral("/one"), a));
    QVERIFY(store.save(QStringLiteral("/two"), b));

    QCOMPARE(store.load(QStringLiteral("/one")).filter, QStringLiteral("alpha"));
    QCOMPARE(store.load(QStringLiteral("/two")).filter, QStringLiteral("beta"));
}

void TestWorkspaceStore::savingAgainReplacesTheEntry()
{
    QTemporaryDir dir;
    WorkspaceStore store(dir.filePath(QStringLiteral("workspaces.json")));

    WorkspaceState first;
    first.filter = QStringLiteral("old");
    first.expandedDirs = {QStringLiteral("docs")};
    first.hasExpandedList = true;
    store.save(QStringLiteral("/p"), first);

    WorkspaceState second;
    second.filter = QStringLiteral("new");
    second.hasExpandedList = true;
    store.save(QStringLiteral("/p"), second);

    const WorkspaceState back = store.load(QStringLiteral("/p"));
    QCOMPARE(back.filter, QStringLiteral("new"));
    QVERIFY(back.expandedDirs.isEmpty());
}

void TestWorkspaceStore::anEmptyExpandedListStillCountsAsSaved()
{
    QTemporaryDir dir;
    WorkspaceStore store(dir.filePath(QStringLiteral("workspaces.json")));

    WorkspaceState state;
    state.hasExpandedList = true; // everything collapsed
    store.save(QStringLiteral("/p"), state);

    QVERIFY(store.load(QStringLiteral("/p")).hasExpandedList);
}

QTEST_APPLESS_MAIN(TestWorkspaceStore)
#include "test_workspace_store.moc"
