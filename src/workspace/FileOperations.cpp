#include "workspace/FileOperations.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

namespace hungryeditor::fileops {

namespace {

Result failure(const QString& message)
{
    return {false, message, {}};
}

Result success(const QString& path)
{
    return {true, {}, path};
}

} // namespace

bool isValidName(const QString& name)
{
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..")) {
        return false;
    }
    return !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'));
}

Result createFile(const QString& parentDir, const QString& name)
{
    if (!isValidName(name)) {
        return failure(QObject::tr("“%1” is not a valid file name.").arg(name));
    }
    const QString path = QDir(parentDir).absoluteFilePath(name);
    if (QFileInfo::exists(path)) {
        return failure(QObject::tr("“%1” already exists.").arg(name));
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return failure(QObject::tr("Could not create “%1”: %2").arg(name, file.errorString()));
    }
    file.close();
    return success(path);
}

Result createFolder(const QString& parentDir, const QString& name)
{
    if (!isValidName(name)) {
        return failure(QObject::tr("“%1” is not a valid folder name.").arg(name));
    }
    const QDir parent(parentDir);
    if (parent.exists(name)) {
        return failure(QObject::tr("“%1” already exists.").arg(name));
    }
    if (!parent.mkdir(name)) {
        return failure(QObject::tr("Could not create the folder “%1”.").arg(name));
    }
    return success(parent.absoluteFilePath(name));
}

Result rename(const QString& path, const QString& newName)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return failure(QObject::tr("“%1” no longer exists.").arg(info.fileName()));
    }
    if (newName == info.fileName()) {
        return success(path);
    }
    if (!isValidName(newName)) {
        return failure(QObject::tr("“%1” is not a valid name.").arg(newName));
    }
    const QString target = info.dir().absoluteFilePath(newName);
    if (QFileInfo::exists(target)) {
        return failure(QObject::tr("“%1” already exists.").arg(newName));
    }
    if (!QFile::rename(path, target)) {
        return failure(QObject::tr("Could not rename “%1”.").arg(info.fileName()));
    }
    return success(target);
}

Result moveToTrash(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return failure(QObject::tr("“%1” no longer exists.").arg(info.fileName()));
    }

    QFile file(path);
    if (file.moveToTrash()) {
        return success(file.fileName()); // now the trashed location
    }

    // No trash available (or it refused) — delete outright.
    const bool removed = info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
    if (!removed) {
        return failure(QObject::tr("Could not delete “%1”.").arg(info.fileName()));
    }
    return success({});
}

} // namespace hungryeditor::fileops
