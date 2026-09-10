#include "markdown/ImageResolver.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QUrl>

namespace hungryeditor::images {

namespace {

/// Undo the attribute escaping `Md4cRenderer` applies to a `src` value, so the
/// result names a real filesystem path.
QString unescapeAttribute(QString value)
{
    value.replace(QLatin1String("&quot;"), QLatin1String("\""));
    value.replace(QLatin1String("&lt;"), QLatin1String("<"));
    value.replace(QLatin1String("&gt;"), QLatin1String(">"));
    value.replace(QLatin1String("&amp;"), QLatin1String("&"));
    return value;
}

/// True when `src` points somewhere the preview can already load on its own.
bool isRemote(const QString& src)
{
    const QString scheme = QUrl(src).scheme().toLower();
    if (scheme.isEmpty() || scheme.size() == 1) {
        return false; // relative path, or a Windows drive letter
    }
    return scheme != QLatin1String("file");
}

/// Resolve `src` (already attribute-unescaped) to an absolute local path, or an
/// empty string when it cannot be resolved.
QString toLocalPath(const QString& src, const QString& documentDir)
{
    if (src.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
        return QUrl(src).toLocalFile();
    }

    QString decoded = QUrl::fromPercentEncoding(src.toUtf8());
    const QFileInfo info(decoded);
    if (info.isAbsolute()) {
        return decoded;
    }
    if (documentDir.isEmpty()) {
        return {};
    }
    return QDir(documentDir).absoluteFilePath(decoded);
}

/// The `src="..."` (or `data-img-*="..."`) attribute text that should stand in
/// for the original inside an `<img>` tag.
QString resolvedAttribute(const QString& rawSrc, const QString& documentDir, qint64 maxBytes)
{
    if (isRemote(rawSrc)) {
        return QStringLiteral("src=\"%1\"").arg(rawSrc);
    }

    const QString path = toLocalPath(unescapeAttribute(rawSrc), documentDir);
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists() || !info.isFile() || !info.isReadable()) {
        return QStringLiteral("data-img-missing=\"%1\"").arg(rawSrc);
    }
    if (info.size() > maxBytes) {
        return QStringLiteral("data-img-toobig=\"%1\"").arg(rawSrc);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("data-img-missing=\"%1\"").arg(rawSrc);
    }
    const QByteArray bytes = file.readAll();
    const QString mime = QMimeDatabase().mimeTypeForFileNameAndData(path, bytes).name();
    if (!mime.startsWith(QLatin1String("image/"))) {
        return QStringLiteral("data-img-missing=\"%1\"").arg(rawSrc);
    }

    return QStringLiteral("src=\"data:%1;base64,%2\"")
        .arg(mime, QString::fromLatin1(bytes.toBase64()));
}

} // namespace

QString inlineLocalImages(const QString& html, const QString& documentDir, qint64 maxBytes)
{
    static const QRegularExpression pattern(QStringLiteral("(<img\\b[^>]*?)\\bsrc=\"([^\"]*)\""));

    QString out;
    qsizetype cursor = 0;
    QRegularExpressionMatchIterator it = pattern.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out += html.mid(cursor, match.capturedStart() - cursor);
        out += match.captured(1);
        out += resolvedAttribute(match.captured(2), documentDir, maxBytes);
        cursor = match.capturedEnd();
    }
    out += html.mid(cursor);
    return out;
}

} // namespace hungryeditor::images
