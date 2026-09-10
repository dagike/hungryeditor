#include "io/AssetWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>

namespace hungryeditor::assets {

QString writePastedImage(const QImage& image, const QString& documentPath,
                         const QString& fallbackDir)
{
    if (image.isNull()) {
        return {};
    }

    const bool haveDocument = !documentPath.isEmpty();
    const QString baseDir =
        haveDocument ? QFileInfo(documentPath).absolutePath() : QDir(fallbackDir).absolutePath();
    if (baseDir.isEmpty()) {
        return {};
    }

    QDir assetsDir(baseDir + QLatin1String("/assets"));
    if (!assetsDir.exists() && !assetsDir.mkpath(QStringLiteral("."))) {
        return {};
    }

    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString absolutePath = assetsDir.filePath(QStringLiteral("pasted-%1.png").arg(stamp));
    if (!image.save(absolutePath, "PNG")) {
        return {};
    }

    return haveDocument ? QDir(baseDir).relativeFilePath(absolutePath) : absolutePath;
}

} // namespace hungryeditor::assets
