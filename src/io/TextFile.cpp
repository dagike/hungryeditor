#include "io/TextFile.h"

#include <utility>

#include <QFile>
#include <QSaveFile>
#include <QStringConverter>

namespace hungryeditor {

namespace {

const QByteArray kUtf8Bom("\xEF\xBB\xBF", 3);
const QByteArray kUtf16LeBom("\xFF\xFE", 2);
const QByteArray kUtf16BeBom("\xFE\xFF", 2);

/// Decode `payload` (BOM already removed) with an explicit converter.
QString decodeWith(const QByteArray& payload, QStringConverter::Encoding codec)
{
    QStringDecoder decoder(codec);
    return decoder.decode(payload);
}

/// Pick the line ending that appears most often, preferring "\n" on a tie or
/// when the text has no newline at all.
LineEnding detectLineEnding(const QString& raw)
{
    const qsizetype crlf = raw.count(QStringLiteral("\r\n"));
    const qsizetype cr = raw.count(QLatin1Char('\r')) - crlf;
    const qsizetype lf = raw.count(QLatin1Char('\n')) - crlf;

    if (crlf > lf && crlf >= cr) {
        return LineEnding::CrLf;
    }
    if (cr > lf && cr > crlf) {
        return LineEnding::Cr;
    }
    return LineEnding::Lf;
}

QString normaliseNewlines(QString raw)
{
    raw.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    raw.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return raw;
}

} // namespace

QString lineEndingText(LineEnding eol)
{
    switch (eol) {
    case LineEnding::CrLf:
        return QStringLiteral("\r\n");
    case LineEnding::Cr:
        return QStringLiteral("\r");
    case LineEnding::Lf:
        break;
    }
    return QStringLiteral("\n");
}

QString lineEndingLabel(LineEnding eol)
{
    switch (eol) {
    case LineEnding::CrLf:
        return QStringLiteral("CRLF");
    case LineEnding::Cr:
        return QStringLiteral("CR");
    case LineEnding::Lf:
        break;
    }
    return QStringLiteral("LF");
}

QString encodingLabel(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Utf8Bom:
        return QStringLiteral("UTF-8 BOM");
    case Encoding::Utf16Le:
        return QStringLiteral("UTF-16 LE");
    case Encoding::Utf16Be:
        return QStringLiteral("UTF-16 BE");
    case Encoding::Latin1:
        return QStringLiteral("ANSI");
    case Encoding::Utf8:
        break;
    }
    return QStringLiteral("UTF-8");
}

TextDocument decodeBytes(const QByteArray& bytes)
{
    TextDocument doc;
    QByteArray payload = bytes;

    if (bytes.startsWith(kUtf8Bom)) {
        doc.encoding = Encoding::Utf8Bom;
        payload = bytes.mid(kUtf8Bom.size());
    } else if (bytes.startsWith(kUtf16LeBom)) {
        doc.encoding = Encoding::Utf16Le;
        payload = bytes.mid(kUtf16LeBom.size());
    } else if (bytes.startsWith(kUtf16BeBom)) {
        doc.encoding = Encoding::Utf16Be;
        payload = bytes.mid(kUtf16BeBom.size());
    } else {
        QStringDecoder strict(QStringConverter::Utf8);
        const QString decoded = strict.decode(bytes);
        if (strict.hasError()) {
            doc.encoding = Encoding::Latin1;
        } else {
            doc.encoding = Encoding::Utf8;
        }
    }

    QString raw;
    switch (doc.encoding) {
    case Encoding::Utf8:
    case Encoding::Utf8Bom:
        raw = decodeWith(payload, QStringConverter::Utf8);
        break;
    case Encoding::Utf16Le:
        raw = decodeWith(payload, QStringConverter::Utf16LE);
        break;
    case Encoding::Utf16Be:
        raw = decodeWith(payload, QStringConverter::Utf16BE);
        break;
    case Encoding::Latin1:
        raw = QString::fromLatin1(payload);
        break;
    }

    doc.lineEnding = detectLineEnding(raw);
    doc.text = normaliseNewlines(std::move(raw));
    return doc;
}

QByteArray encodeDocument(const TextDocument& doc)
{
    QString text = doc.text;
    if (doc.lineEnding != LineEnding::Lf) {
        text.replace(QLatin1Char('\n'), lineEndingText(doc.lineEnding));
    }

    switch (doc.encoding) {
    case Encoding::Utf8:
        return text.toUtf8();
    case Encoding::Utf8Bom:
        return kUtf8Bom + text.toUtf8();
    case Encoding::Utf16Le: {
        QStringEncoder encoder(QStringConverter::Utf16LE);
        return kUtf16LeBom + QByteArray(encoder.encode(text));
    }
    case Encoding::Utf16Be: {
        QStringEncoder encoder(QStringConverter::Utf16BE);
        return kUtf16BeBom + QByteArray(encoder.encode(text));
    }
    case Encoding::Latin1:
        return text.toLatin1();
    }
    return text.toUtf8();
}

TextDocument loadFile(const QString& path, FileError* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = {false, file.errorString()};
        }
        return {};
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        if (error != nullptr) {
            *error = {false, file.errorString()};
        }
        return {};
    }
    if (error != nullptr) {
        *error = {};
    }
    return decodeBytes(bytes);
}

bool saveFile(const QString& path, const TextDocument& doc, FileError* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = {false, file.errorString()};
        }
        return false;
    }
    const QByteArray bytes = encodeDocument(doc);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error != nullptr) {
            *error = {false, file.errorString()};
        }
        return false;
    }
    if (error != nullptr) {
        *error = {};
    }
    return true;
}

} // namespace hungryeditor
