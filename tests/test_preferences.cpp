// Coverage for preferences.json persistence.

#include <QTemporaryDir>
#include <QtTest>

#include "io/Preferences.h"

using hungryeditor::Preferences;
using hungryeditor::PreferencesStore;

class TestPreferences : public QObject
{
    Q_OBJECT

private slots:
    void loadFromAMissingFileReturnsDefaults();
    void saveThenLoadRoundTrips();
};

void TestPreferences::loadFromAMissingFileReturnsDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PreferencesStore store(dir.filePath(QStringLiteral("preferences.json")));

    const Preferences preferences = store.load();
    QVERIFY(preferences.fontFamily.isEmpty());
    QCOMPARE(preferences.fontSize, 11);
    QCOMPARE(preferences.tabWidth, 4);
    QVERIFY(!preferences.wordWrap);
}

void TestPreferences::saveThenLoadRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A nested path exercises the on-demand directory creation.
    PreferencesStore store(dir.filePath(QStringLiteral("state/preferences.json")));

    Preferences preferences;
    preferences.fontFamily = QStringLiteral("Monospace");
    preferences.fontSize = 14;
    preferences.tabWidth = 2;
    preferences.wordWrap = true;
    QVERIFY(store.save(preferences));

    const Preferences loaded = store.load();
    QCOMPARE(loaded.fontFamily, QStringLiteral("Monospace"));
    QCOMPARE(loaded.fontSize, 14);
    QCOMPARE(loaded.tabWidth, 2);
    QVERIFY(loaded.wordWrap);
}

QTEST_APPLESS_MAIN(TestPreferences)
#include "test_preferences.moc"
