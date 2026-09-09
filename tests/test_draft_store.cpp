// Coverage for the crash-recovery draft directory.

#include <QTemporaryDir>
#include <QtTest>

#include "io/DraftStore.h"

using hungryeditor::Draft;
using hungryeditor::DraftStore;
using hungryeditor::Encoding;
using hungryeditor::LineEnding;

class TestDraftStore : public QObject
{
    Q_OBJECT

private slots:
    void writeThenLoadRoundTrips();
    void loadReturnsDraftsOldestFirst();
    void removeDeletesASingleDraft();
    void clearEmptiesTheDirectory();
    void loadFromAMissingDirectoryIsEmpty();
};

void TestDraftStore::writeThenLoadRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DraftStore store(dir.path());

    Draft draft;
    draft.id = QStringLiteral("aaaa1111");
    draft.originalPath = QStringLiteral("/home/user/note.md");
    draft.text = QStringLiteral("line one\nline two\n");
    draft.encoding = Encoding::Latin1;
    draft.lineEnding = LineEnding::CrLf;
    QVERIFY(store.write(draft));

    const QList<Draft> loaded = store.loadAll();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().id, draft.id);
    QCOMPARE(loaded.first().originalPath, draft.originalPath);
    QCOMPARE(loaded.first().text, draft.text);
    QCOMPARE(loaded.first().encoding, Encoding::Latin1);
    QCOMPARE(loaded.first().lineEnding, LineEnding::CrLf);
}

void TestDraftStore::loadReturnsDraftsOldestFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DraftStore store(dir.path());

    Draft older;
    older.id = QStringLiteral("older");
    older.text = QStringLiteral("older\n");
    QVERIFY(store.write(older));

    QTest::qWait(1100); // filesystem mtime resolution

    Draft newer;
    newer.id = QStringLiteral("newer");
    newer.text = QStringLiteral("newer\n");
    QVERIFY(store.write(newer));

    const QList<Draft> loaded = store.loadAll();
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.first().id, QStringLiteral("older"));
    QCOMPARE(loaded.last().id, QStringLiteral("newer"));
}

void TestDraftStore::removeDeletesASingleDraft()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DraftStore store(dir.path());

    Draft a;
    a.id = QStringLiteral("keep");
    a.text = QStringLiteral("keep\n");
    Draft b;
    b.id = QStringLiteral("drop");
    b.text = QStringLiteral("drop\n");
    QVERIFY(store.write(a));
    QVERIFY(store.write(b));

    store.remove(QStringLiteral("drop"));

    const QList<Draft> loaded = store.loadAll();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().id, QStringLiteral("keep"));
}

void TestDraftStore::clearEmptiesTheDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DraftStore store(dir.path());

    Draft draft;
    draft.id = QStringLiteral("x");
    draft.text = QStringLiteral("x\n");
    QVERIFY(store.write(draft));

    store.clear();
    QVERIFY(store.loadAll().isEmpty());
}

void TestDraftStore::loadFromAMissingDirectoryIsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DraftStore store(dir.path() + QStringLiteral("/not-created-yet"));
    QVERIFY(store.loadAll().isEmpty());
}

QTEST_MAIN(TestDraftStore)
#include "test_draft_store.moc"
