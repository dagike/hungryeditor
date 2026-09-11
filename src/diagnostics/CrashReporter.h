#pragma once

#include <QString>

namespace hungryeditor::crashreporter {

/// Install a fatal-signal (POSIX) / unhandled-exception (Windows) handler
/// that writes a local diagnostic report — timestamp, a raw backtrace, app
/// and Qt version — to `directory`/crashes/crash-<timestamp>.txt, then lets
/// the process die exactly as it would have otherwise (the previous
/// handler, if any, still runs afterward; a debugger or the OS's own crash
/// dialog is never pre-empted).
///
/// Nothing here is ever transmitted anywhere — this only ever writes a local
/// file. Call this at most once, as early in main() as practical, and only
/// when the user has opted in (Preferences::crashReportingEnabled):
/// this touches process-wide signal disposition, so it is not something to
/// install and uninstall casually, and it has no effect on anything that
/// happens before it runs.
///
/// `directory`/crashes is created (including any missing parent directories)
/// if it does not already exist.
void install(const QString& directory);

} // namespace hungryeditor::crashreporter
