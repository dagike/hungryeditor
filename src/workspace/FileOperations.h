#pragma once

#include <QString>

namespace hungryeditor::fileops {

/// Outcome of a filesystem operation: `ok` with the resulting `path`, or a
/// human-readable `error` describing why nothing changed.
struct Result
{
    bool ok = false;
    QString error;
    QString path;
};

/// True when `name` is a usable single path segment — non-empty, not "." or
/// "..", and free of directory separators.
bool isValidName(const QString& name);

/// Create an empty file `name` inside `parentDir`.
Result createFile(const QString& parentDir, const QString& name);

/// Create a directory `name` inside `parentDir`.
Result createFolder(const QString& parentDir, const QString& name);

/// Rename the file or directory at `path` to the bare name `newName`, keeping
/// it in the same parent directory. A no-op (still ok) when `newName` matches
/// the current name.
Result rename(const QString& path, const QString& newName);

/// Send the file or directory at `path` to the trash, falling back to a
/// permanent recursive delete where the platform has no trash.
Result moveToTrash(const QString& path);

} // namespace hungryeditor::fileops
