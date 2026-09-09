#include "editor/Document.h"

#include <utility>

#include <QFileInfo>

namespace hungryeditor {

Document::Document(Scintilla::IDocumentEditable* pointer, QString path, int untitledNumber)
    : pointer_(pointer), path_(std::move(path)), untitledNumber_(untitledNumber)
{
}

void Document::recordDiskState(qint64 size, const QDateTime& modified)
{
    diskSize_ = size;
    diskModified_ = modified;
}

void Document::clearDiskState()
{
    diskSize_ = -1;
    diskModified_ = QDateTime();
}

bool Document::matchesDiskState(qint64 size, const QDateTime& modified) const
{
    return diskSize_ == size && diskModified_ == modified;
}

QString Document::displayName() const
{
    if (!path_.isEmpty()) {
        return QFileInfo(path_).fileName();
    }
    if (untitledNumber_ == 0) {
        return QStringLiteral("Untitled");
    }
    return QStringLiteral("Untitled %1").arg(untitledNumber_);
}

} // namespace hungryeditor
