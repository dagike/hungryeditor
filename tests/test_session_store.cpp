// Coverage for session.json persistence.

#include <QTemporaryDir>
#include <QtTest>

#include "io/SessionStore.h"

using hungryeditor::Session;
using hungryeditor::SessionDocument;
using hungryeditor::SessionStore;

class TestSessionStore : public QObject
{
    Q_OBJECT

private slots:
    void loadFromAMissingFileIsInvalid();
    void saveThenLoadRoundTrips();
    void clearRemovesTheFile();
};

void TestSessionStore::loadFromAMissingFileIsInvalid()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SessionStore store(dir.filePath(QStringLiteral("session.json")));
    QVERIFY(!store.load().valid);
}

void TestSessionStore::saveThenLoadRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A nested path exercises the on-demand directory creation.
    SessionStore store(dir.filePath(QStringLiteral("state/session.json")));

    Session session;
    session.valid = true;
    session.windowGeometry = QByteArray::fromHex("deadbeef00c0ffee");
    session.currentIndex = 1;
    session.documents.append({QStringLiteral("/tmp/a.md"), QString(), 3, 7, 2});
    session.documents.append({QString(), QStringLiteral("draft-9"), 0, 0, 0});
    QVERIFY(store.save(session));

    const Session loaded = store.load();
    QVERIFY(loaded.valid);
    QCOMPARE(loaded.windowGeometry, session.windowGeometry);
    QCOMPARE(loaded.currentIndex, 1);
    QCOMPARE(loaded.documents.size(), 2);
    QCOMPARE(loaded.documents.at(0).path, QStringLiteral("/tmp/a.md"));
    QCOMPARE(loaded.documents.at(0).caretLine, 3);
    QCOMPARE(loaded.documents.at(0).caretColumn, 7);
    QCOMPARE(loaded.documents.at(0).firstVisibleLine, 2);
    QVERIFY(loaded.documents.at(1).path.isEmpty());
    QCOMPARE(loaded.documents.at(1).draftId, QStringLiteral("draft-9"));
}

void TestSessionStore::clearRemovesTheFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SessionStore store(dir.filePath(QStringLiteral("session.json")));

    Session session;
    session.valid = true;
    QVERIFY(store.save(session));
    QVERIFY(store.load().valid);

    store.clear();
    QVERIFY(!store.load().valid);
}

QTEST_MAIN(TestSessionStore)
#include "test_session_store.moc"
