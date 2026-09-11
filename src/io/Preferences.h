#pragma once

#include <QString>

namespace hungryeditor {

/// User-configurable editor settings, independent of any open document or
/// window layout. Persisted separately from session.json, which only ever
/// describes the last clean exit.
struct Preferences
{
    QString fontFamily; ///< empty selects the platform's default fixed-width font
    int fontSize = 11;
    int tabWidth = 4;
    bool wordWrap = false;
    /// Opt-in, off by default: write a local crash report (see
    /// diagnostics/CrashReporter.h) on a fatal signal/exception. Never
    /// transmitted anywhere — the report only ever goes to a file on disk.
    bool crashReportingEnabled = false;
};

/// Reads and writes the single preferences.json holding the user's settings.
/// Its location is injected so the running app and the test suite never share
/// one.
class PreferencesStore
{
public:
    explicit PreferencesStore(QString filePath);

    QString filePath() const { return filePath_; }

    /// The saved preferences, or defaults when the file is missing or
    /// unreadable.
    Preferences load() const;

    /// Write `preferences` atomically, creating the parent directory.
    bool save(const Preferences& preferences) const;

private:
    QString filePath_;
};

} // namespace hungryeditor
