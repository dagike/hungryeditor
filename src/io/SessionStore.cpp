#include "io/SessionStore.h"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace hungryeditor {

SessionStore::SessionStore(QString filePath) : filePath_(std::move(filePath))
{
}

Session SessionStore::load() const
{
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.isEmpty()) {
        return {};
    }

    Session session;
    session.valid = true;
    session.windowGeometry =
        QByteArray::fromBase64(root.value(QStringLiteral("geometry")).toString().toLatin1());
    session.currentIndex = root.value(QStringLiteral("current")).toInt();
    const QJsonArray documents = root.value(QStringLiteral("documents")).toArray();
    for (const QJsonValue& value : documents) {
        const QJsonObject entry = value.toObject();
        SessionDocument document;
        document.path = entry.value(QStringLiteral("path")).toString();
        document.draftId = entry.value(QStringLiteral("draftId")).toString();
        const QJsonArray caret = entry.value(QStringLiteral("caret")).toArray();
        if (caret.size() == 2) {
            document.caretLine = caret.at(0).toInt();
            document.caretColumn = caret.at(1).toInt();
        }
        document.firstVisibleLine = entry.value(QStringLiteral("scroll")).toInt();
        session.documents.append(document);
    }
    return session;
}

bool SessionStore::save(const Session& session) const
{
    QJsonArray documents;
    for (const SessionDocument& document : session.documents) {
        QJsonObject entry;
        entry.insert(QStringLiteral("path"), document.path);
        entry.insert(QStringLiteral("draftId"), document.draftId);
        entry.insert(QStringLiteral("caret"), QJsonArray{document.caretLine, document.caretColumn});
        entry.insert(QStringLiteral("scroll"), document.firstVisibleLine);
        documents.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("geometry"), QString::fromLatin1(session.windowGeometry.toBase64()));
    root.insert(QStringLiteral("current"), session.currentIndex);
    root.insert(QStringLiteral("documents"), documents);

    QDir().mkpath(QFileInfo(filePath_).absolutePath());
    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

void SessionStore::clear() const
{
    QFile::remove(filePath_);
}

} // namespace hungryeditor
