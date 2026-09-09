// Coverage for the recent-files list.

#include <QTemporaryDir>
#include <QtTest>

#include "io/RecentFiles.h"

using hungryeditor::RecentFile;
using hungryeditor::RecentFiles;

class TestRecentFiles : public QObject
{
    Q_OBJECT

private slots:
    void newestFirstAndDeduped();
    void pinnedEntriesLeadAndSurviveEviction();
    void clearUnpinnedKeepsPins();
    void forgetRemovesAnyEntry();
    void roundTripsThroughDisk();
};

void TestRecentFiles::newestFirstAndDeduped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RecentFiles recent(dir.filePath(QStringLiteral("recent.json")));

    recent.noteOpened(QStringLiteral("/x/a.md"));
    recent.noteOpened(QStringLiteral("/x/b.md"));
    recent.noteOpened(QStringLiteral("/x/a.md")); // bump a back to the front

    const QList<RecentFile> entries = recent.entries();
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).path, QStringLiteral("/x/a.md"));
    QCOMPARE(entries.at(1).path, QStringLiteral("/x/b.md"));
}

void TestRecentFiles::pinnedEntriesLeadAndSurviveEviction()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RecentFiles recent(dir.filePath(QStringLiteral("recent.json")));

    recent.noteOpened(QStringLiteral("/x/pinme.md"));
    recent.setPinned(QStringLiteral("/x/pinme.md"), true);

    for (int i = 0; i < RecentFiles::kMaxRecent + 5; ++i) {
        recent.noteOpened(QStringLiteral("/x/f%1.md").arg(i));
    }

    const QList<RecentFile> entries = recent.entries();
    QVERIFY(entries.first().pinned);
    QCOMPARE(entries.first().path, QStringLiteral("/x/pinme.md"));

    int unpinned = 0;
    for (const RecentFile& entry : entries) {
        if (!entry.pinned) {
            ++unpinned;
        }
    }
    QCOMPARE(unpinned, RecentFiles::kMaxRecent);
}

void TestRecentFiles::clearUnpinnedKeepsPins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RecentFiles recent(dir.filePath(QStringLiteral("recent.json")));

    recent.noteOpened(QStringLiteral("/x/a.md"));
    recent.noteOpened(QStringLiteral("/x/b.md"));
    recent.setPinned(QStringLiteral("/x/a.md"), true);

    recent.clearUnpinned();

    const QList<RecentFile> entries = recent.entries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("/x/a.md"));
    QVERIFY(entries.first().pinned);
}

void TestRecentFiles::forgetRemovesAnyEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RecentFiles recent(dir.filePath(QStringLiteral("recent.json")));

    recent.noteOpened(QStringLiteral("/x/a.md"));
    recent.setPinned(QStringLiteral("/x/a.md"), true);
    recent.noteOpened(QStringLiteral("/x/b.md"));

    recent.forget(QStringLiteral("/x/a.md"));
    recent.forget(QStringLiteral("/x/b.md"));
    QVERIFY(recent.entries().isEmpty());
}

void TestRecentFiles::roundTripsThroughDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("state/recent.json"));
    {
        RecentFiles recent(path);
        recent.noteOpened(QStringLiteral("/x/a.md"));
        recent.noteOpened(QStringLiteral("/x/b.md"));
        recent.setPinned(QStringLiteral("/x/b.md"), true);
        QVERIFY(recent.save());
    }

    RecentFiles reloaded(path); // the constructor loads
    const QList<RecentFile> entries = reloaded.entries();
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).path, QStringLiteral("/x/b.md"));
    QVERIFY(entries.at(0).pinned);
    QCOMPARE(entries.at(1).path, QStringLiteral("/x/a.md"));
    QVERIFY(!entries.at(1).pinned);
}

QTEST_MAIN(TestRecentFiles)
#include "test_recent_files.moc"
