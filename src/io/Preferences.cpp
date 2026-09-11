#include "io/Preferences.h"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace hungryeditor {

PreferencesStore::PreferencesStore(QString filePath) : filePath_(std::move(filePath))
{
}

Preferences PreferencesStore::load() const
{
    Preferences preferences;

    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        return preferences;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.isEmpty()) {
        return preferences;
    }

    preferences.fontFamily = root.value(QStringLiteral("fontFamily")).toString();
    if (root.contains(QStringLiteral("fontSize"))) {
        preferences.fontSize = root.value(QStringLiteral("fontSize")).toInt(preferences.fontSize);
    }
    if (root.contains(QStringLiteral("tabWidth"))) {
        preferences.tabWidth = root.value(QStringLiteral("tabWidth")).toInt(preferences.tabWidth);
    }
    preferences.wordWrap = root.value(QStringLiteral("wordWrap")).toBool(preferences.wordWrap);
    preferences.crashReportingEnabled = root.value(QStringLiteral("crashReportingEnabled"))
                                            .toBool(preferences.crashReportingEnabled);
    return preferences;
}

bool PreferencesStore::save(const Preferences& preferences) const
{
    QJsonObject root;
    root.insert(QStringLiteral("fontFamily"), preferences.fontFamily);
    root.insert(QStringLiteral("fontSize"), preferences.fontSize);
    root.insert(QStringLiteral("tabWidth"), preferences.tabWidth);
    root.insert(QStringLiteral("wordWrap"), preferences.wordWrap);
    root.insert(QStringLiteral("crashReportingEnabled"), preferences.crashReportingEnabled);

    QDir().mkpath(QFileInfo(filePath_).absolutePath());
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

} // namespace hungryeditor
