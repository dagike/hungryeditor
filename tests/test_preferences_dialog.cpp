// Coverage for the Preferences dialog's get/set round-trip.

#include <QFontDatabase>
#include <QtTest>

#include "io/Preferences.h"
#include "ui/PreferencesDialog.h"

using hungryeditor::Preferences;
using hungryeditor::PreferencesDialog;

class TestPreferencesDialog : public QObject
{
    Q_OBJECT

private slots:
    void seedsFromAndReadsBackPreferences();
    void anEmptyFontFamilyLeavesTheDefaultSelectionAlone();
};

void TestPreferencesDialog::seedsFromAndReadsBackPreferences()
{
    Preferences preferences;
    preferences.fontFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    preferences.fontSize = 18;
    preferences.tabWidth = 8;
    preferences.wordWrap = true;

    PreferencesDialog dialog;
    dialog.setPreferences(preferences);

    const Preferences readBack = dialog.preferences();
    QCOMPARE(readBack.fontFamily, preferences.fontFamily);
    QCOMPARE(readBack.fontSize, 18);
    QCOMPARE(readBack.tabWidth, 8);
    QVERIFY(readBack.wordWrap);
}

void TestPreferencesDialog::anEmptyFontFamilyLeavesTheDefaultSelectionAlone()
{
    PreferencesDialog dialog;
    dialog.setPreferences(Preferences{}); // fontFamily left empty

    // No crash, and some concrete family comes back (whatever the combo box
    // defaulted to) rather than staying empty.
    QVERIFY(!dialog.preferences().fontFamily.isEmpty());
}

QTEST_MAIN(TestPreferencesDialog)
#include "test_preferences_dialog.moc"
