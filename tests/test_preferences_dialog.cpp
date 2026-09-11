// Coverage for the Preferences dialog's get/set round-trip.

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
    // Use a family the combo box already offers in this environment (its own
    // default selection) rather than one guessed via QFontDatabase — font
    // enumeration varies too much across CI platforms (a Windows runner with
    // no deployed font directory falls back to "Sans Serif" regardless of
    // what family name is requested) for an externally-sourced name to
    // reliably round-trip. This test is about the dialog's get/set plumbing,
    // not font-matching behaviour.
    PreferencesDialog probe;
    const QString selectableFamily = probe.preferences().fontFamily;
    QVERIFY(!selectableFamily.isEmpty());

    Preferences preferences;
    preferences.fontFamily = selectableFamily;
    preferences.fontSize = 18;
    preferences.tabWidth = 8;
    preferences.wordWrap = true;
    preferences.crashReportingEnabled = true;

    PreferencesDialog dialog;
    dialog.setPreferences(preferences);

    const Preferences readBack = dialog.preferences();
    QCOMPARE(readBack.fontFamily, selectableFamily);
    QCOMPARE(readBack.fontSize, 18);
    QCOMPARE(readBack.tabWidth, 8);
    QVERIFY(readBack.wordWrap);
    QVERIFY(readBack.crashReportingEnabled);
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
