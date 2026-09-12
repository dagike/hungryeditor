#include "preview/PreviewProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace hungryeditor::preview_protocol {

namespace {

QString encode(const QString& type, QJsonObject payload = {})
{
    payload.insert(QStringLiteral("type"), type);
    return QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

} // namespace

PageMessage decodePageMessage(const QString& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    const QJsonObject object = doc.object();
    const QString type = object.value(QStringLiteral("type")).toString();

    PageMessage message;
    if (type == QStringLiteral("hello")) {
        message.type = PageMessageType::Hello;
    } else if (type == QStringLiteral("ready")) {
        message.type = PageMessageType::Ready;
    } else if (type == QStringLiteral("scroll")) {
        message.type = PageMessageType::Scroll;
        message.line = object.value(QStringLiteral("line")).toInt();
    } else if (type == QStringLiteral("headingClick")) {
        message.type = PageMessageType::HeadingClick;
        message.line = object.value(QStringLiteral("line")).toInt();
    } else if (type == QStringLiteral("taskToggle")) {
        message.type = PageMessageType::TaskToggle;
        message.line = object.value(QStringLiteral("line")).toInt();
        message.checked = object.value(QStringLiteral("checked")).toBool();
    }
    return message;
}

QString encodeTheme(const QString& css)
{
    return encode(QStringLiteral("theme"), QJsonObject{{QStringLiteral("css"), css}});
}

QString encodeContent(const QString& html)
{
    return encode(QStringLiteral("content"), QJsonObject{{QStringLiteral("html"), html}});
}

QString encodeScrollToLine(int line)
{
    return encode(QStringLiteral("scrollToLine"), QJsonObject{{QStringLiteral("line"), line}});
}

} // namespace hungryeditor::preview_protocol
