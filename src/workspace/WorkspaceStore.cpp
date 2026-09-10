#include "workspace/WorkspaceStore.h"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace hungryeditor {

namespace {

QJsonObject readRoot(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

WorkspaceStore::WorkspaceStore(QString filePath) : filePath_(std::move(filePath))
{
}

WorkspaceState WorkspaceStore::load(const QString& folder) const
{
    const QJsonObject entry = readRoot(filePath_).value(folder).toObject();
    if (entry.isEmpty()) {
        return {};
    }

    WorkspaceState state;
    state.filter = entry.value(QStringLiteral("filter")).toString();
    if (entry.contains(QStringLiteral("expanded"))) {
        state.hasExpandedList = true;
        for (const auto& value : entry.value(QStringLiteral("expanded")).toArray()) {
            state.expandedDirs.append(value.toString());
        }
    }
    return state;
}

bool WorkspaceStore::save(const QString& folder, const WorkspaceState& state) const
{
    QJsonObject root = readRoot(filePath_);

    QJsonObject entry;
    entry.insert(QStringLiteral("filter"), state.filter);
    if (state.hasExpandedList) {
        entry.insert(QStringLiteral("expanded"), QJsonArray::fromStringList(state.expandedDirs));
    }
    root.insert(folder, entry);

    QDir().mkpath(QFileInfo(filePath_).absolutePath());
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

} // namespace hungryeditor
