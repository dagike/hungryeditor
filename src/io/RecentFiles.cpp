#include "io/RecentFiles.h"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace hungryeditor {

RecentFiles::RecentFiles(QString filePath) : filePath_(std::move(filePath))
{
    load();
}

int RecentFiles::indexOf(const QString& absolutePath) const
{
    for (qsizetype i = 0; i < entries_.size(); ++i) {
        if (entries_.at(i).path == absolutePath) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int RecentFiles::pinnedCount() const
{
    int n = 0;
    for (const RecentFile& entry : entries_) {
        if (entry.pinned) {
            ++n;
        }
    }
    return n;
}

void RecentFiles::trimUnpinned()
{
    // Unpinned entries are always the tail of the list.
    while (entries_.size() - pinnedCount() > kMaxRecent && !entries_.last().pinned) {
        entries_.removeLast();
    }
}

void RecentFiles::noteOpened(const QString& path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (absolute.isEmpty()) {
        return;
    }

    const int existing = indexOf(absolute);
    if (existing >= 0) {
        if (entries_.at(existing).pinned) {
            return; // a pinned entry keeps its fixed slot
        }
        entries_.removeAt(existing);
    }
    entries_.insert(pinnedCount(), {absolute, false});
    trimUnpinned();
}

void RecentFiles::forget(const QString& path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    entries_.removeIf([&absolute](const RecentFile& entry) { return entry.path == absolute; });
}

void RecentFiles::setPinned(const QString& path, bool pinned)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (absolute.isEmpty()) {
        return;
    }

    const int existing = indexOf(absolute);
    if (existing >= 0) {
        if (entries_.at(existing).pinned == pinned) {
            return;
        }
        entries_.removeAt(existing);
    } else if (!pinned) {
        return; // nothing to unpin
    }

    // A newly pinned entry joins the end of the pinned block; a newly unpinned
    // one goes to the front of the unpinned block.
    entries_.insert(pinnedCount(), {absolute, pinned});
    if (!pinned) {
        trimUnpinned();
    }
}

bool RecentFiles::isPinned(const QString& path) const
{
    const int existing = indexOf(QFileInfo(path).absoluteFilePath());
    return existing >= 0 && entries_.at(existing).pinned;
}

void RecentFiles::clearUnpinned()
{
    entries_.removeIf([](const RecentFile& entry) { return !entry.pinned; });
}

void RecentFiles::load()
{
    entries_.clear();

    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonArray files =
        QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("files")).toArray();

    QList<RecentFile> pinned;
    QList<RecentFile> unpinned;
    for (const auto& value : files) {
        const QJsonObject entry = value.toObject();
        const QString path = entry.value(QStringLiteral("path")).toString();
        if (path.isEmpty()) {
            continue;
        }
        RecentFile recent{path, entry.value(QStringLiteral("pinned")).toBool()};
        (recent.pinned ? pinned : unpinned).append(recent);
    }
    // Rebuild the pinned-first invariant regardless of how the file was ordered.
    entries_ = pinned + unpinned;
}

bool RecentFiles::save() const
{
    QJsonArray files;
    for (const RecentFile& entry : entries_) {
        QJsonObject object;
        object.insert(QStringLiteral("path"), entry.path);
        object.insert(QStringLiteral("pinned"), entry.pinned);
        files.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("files"), files);

    QDir().mkpath(QFileInfo(filePath_).absolutePath());
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

} // namespace hungryeditor
