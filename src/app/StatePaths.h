#pragma once

#include <QString>

namespace hungryeditor {

/// Base directory for this app's persisted state — recovery drafts, the
/// session file, preferences and crash reports. Falls back to a temp path
/// when the platform offers none. Shared between MainWindow (which points
/// its stores at it) and main() (which needs it before a MainWindow exists,
/// to decide whether to install the crash reporter).
QString defaultStateDirectory();

} // namespace hungryeditor
