#include "io/DraftStore.h"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace hungryeditor {

namespace {

QString encodingKey(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Utf8:
        return QStringLiteral("utf-8");
    case Encoding::Utf8Bom:
        return QStringLiteral("utf-8-bom");
    case Encoding::Utf16Le:
        return QStringLiteral("utf-16le");
    case Encoding::Utf16Be:
        return QStringLiteral("utf-16be");
    case Encoding::Latin1:
        return QStringLiteral("latin1");
    }
    return QStringLiteral("utf-8");
}

Encoding encodingFromKey(const QString& key)
{
    if (key == QLatin1String("utf-8-bom")) {
        return Encoding::Utf8Bom;
    }
    if (key == QLatin1String("utf-16le")) {
        return Encoding::Utf16Le;
    }
    if (key == QLatin1String("utf-16be")) {
        return Encoding::Utf16Be;
    }
    if (key == QLatin1String("latin1")) {
        return Encoding::Latin1;
    }
    return Encoding::Utf8;
}

QString lineEndingKey(LineEnding lineEnding)
{
    switch (lineEnding) {
    case LineEnding::Lf:
        return QStringLiteral("lf");
    case LineEnding::CrLf:
        return QStringLiteral("crlf");
    case LineEnding::Cr:
        return QStringLiteral("cr");
    }
    return QStringLiteral("lf");
}

LineEnding lineEndingFromKey(const QString& key)
{
    if (key == QLatin1String("crlf")) {
        return LineEnding::CrLf;
    }
    if (key == QLatin1String("cr")) {
        return LineEnding::Cr;
    }
    return LineEnding::Lf;
}

QString fileFor(const QString& directory, const QString& id)
{
    return directory + QLatin1Char('/') + id + QLatin1String(".json");
}

} // namespace

DraftStore::DraftStore(QString directory) : directory_(std::move(directory))
{
}

bool DraftStore::write(const Draft& draft)
{
    if (draft.id.isEmpty()) {
        return false;
    }
    QDir().mkpath(directory_);

    QJsonObject root;
    root.insert(QStringLiteral("id"), draft.id);
    root.insert(QStringLiteral("path"), draft.originalPath);
    root.insert(QStringLiteral("encoding"), encodingKey(draft.encoding));
    root.insert(QStringLiteral("lineEnding"), lineEndingKey(draft.lineEnding));
    root.insert(QStringLiteral("text"), draft.text);

    QSaveFile file(fileFor(directory_, draft.id));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

void DraftStore::remove(const QString& id)
{
    if (!id.isEmpty()) {
        QFile::remove(fileFor(directory_, id));
    }
}

QList<Draft> DraftStore::loadAll() const
{
    QDir dir(directory_);
    if (!dir.exists()) {
        return {};
    }

    QList<Draft> drafts;
    const QFileInfoList entries =
        dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time | QDir::Reversed);
    for (const QFileInfo& entry : entries) {
        QFile file(entry.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        if (root.isEmpty()) {
            continue;
        }
        Draft draft;
        draft.id = root.value(QStringLiteral("id")).toString(entry.completeBaseName());
        draft.originalPath = root.value(QStringLiteral("path")).toString();
        draft.text = root.value(QStringLiteral("text")).toString();
        draft.encoding = encodingFromKey(root.value(QStringLiteral("encoding")).toString());
        draft.lineEnding = lineEndingFromKey(root.value(QStringLiteral("lineEnding")).toString());
        drafts.append(draft);
    }
    return drafts;
}

void DraftStore::clear()
{
    QDir dir(directory_);
    const QStringList names = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString& name : names) {
        dir.remove(name);
    }
}

void DraftStore::retainOnly(const QSet<QString>& ids)
{
    QDir dir(directory_);
    const QStringList names = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString& name : names) {
        if (!ids.contains(QFileInfo(name).completeBaseName())) {
            dir.remove(name);
        }
    }
}

} // namespace hungryeditor
