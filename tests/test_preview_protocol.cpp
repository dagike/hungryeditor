// Coverage for the preview's wire protocol: pure JSON encode/decode logic,
// with no engine (QWebChannel/WebView2) involved. Once this passes, both
// backends can be trusted to speak the same language, since they never
// build or parse a message any other way.

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "preview/PreviewProtocol.h"

using hungryeditor::preview_protocol::decodePageMessage;
using hungryeditor::preview_protocol::encodeContent;
using hungryeditor::preview_protocol::encodeScrollToLine;
using hungryeditor::preview_protocol::encodeTheme;
using hungryeditor::preview_protocol::PageMessageType;

class TestPreviewProtocol : public QObject
{
    Q_OBJECT

private slots:
    void encodesThemeAsJson();
    void encodesContentAsJson();
    void encodesScrollToLineAsJson();
    void decodesHello();
    void decodesReady();
    void decodesScrollWithItsLine();
    void decodesHeadingClickWithItsLine();
    void decodesTaskToggleWithLineAndCheckedState();
    void decodesMalformedJsonAsUnknown();
    void decodesAnUnrecognisedTypeAsUnknown();
    void decodesNonObjectJsonAsUnknown();
};

void TestPreviewProtocol::encodesThemeAsJson()
{
    const QString json = encodeTheme(QStringLiteral("body { color: red; }"));
    const auto decoded = QJsonDocument::fromJson(json.toUtf8()).object();
    QCOMPARE(decoded.value("type").toString(), QStringLiteral("theme"));
    QCOMPARE(decoded.value("css").toString(), QStringLiteral("body { color: red; }"));
}

void TestPreviewProtocol::encodesContentAsJson()
{
    const QString json = encodeContent(QStringLiteral("<p>hi</p>"));
    const auto decoded = QJsonDocument::fromJson(json.toUtf8()).object();
    QCOMPARE(decoded.value("type").toString(), QStringLiteral("content"));
    QCOMPARE(decoded.value("html").toString(), QStringLiteral("<p>hi</p>"));
}

void TestPreviewProtocol::encodesScrollToLineAsJson()
{
    const QString json = encodeScrollToLine(42);
    const auto decoded = QJsonDocument::fromJson(json.toUtf8()).object();
    QCOMPARE(decoded.value("type").toString(), QStringLiteral("scrollToLine"));
    QCOMPARE(decoded.value("line").toInt(), 42);
}

void TestPreviewProtocol::decodesHello()
{
    const auto message = decodePageMessage(QStringLiteral(R"({"type":"hello"})"));
    QCOMPARE(message.type, PageMessageType::Hello);
}

void TestPreviewProtocol::decodesReady()
{
    const auto message = decodePageMessage(QStringLiteral(R"({"type":"ready"})"));
    QCOMPARE(message.type, PageMessageType::Ready);
}

void TestPreviewProtocol::decodesScrollWithItsLine()
{
    const auto message = decodePageMessage(QStringLiteral(R"({"type":"scroll","line":7})"));
    QCOMPARE(message.type, PageMessageType::Scroll);
    QCOMPARE(message.line, 7);
}

void TestPreviewProtocol::decodesHeadingClickWithItsLine()
{
    const auto message = decodePageMessage(QStringLiteral(R"({"type":"headingClick","line":3})"));
    QCOMPARE(message.type, PageMessageType::HeadingClick);
    QCOMPARE(message.line, 3);
}

void TestPreviewProtocol::decodesTaskToggleWithLineAndCheckedState()
{
    const auto message =
        decodePageMessage(QStringLiteral(R"({"type":"taskToggle","line":5,"checked":true})"));
    QCOMPARE(message.type, PageMessageType::TaskToggle);
    QCOMPARE(message.line, 5);
    QCOMPARE(message.checked, true);
}

void TestPreviewProtocol::decodesMalformedJsonAsUnknown()
{
    const auto message = decodePageMessage(QStringLiteral("not json at all"));
    QCOMPARE(message.type, PageMessageType::Unknown);
}

void TestPreviewProtocol::decodesAnUnrecognisedTypeAsUnknown()
{
    const auto message = decodePageMessage(QStringLiteral(R"({"type":"somethingElse"})"));
    QCOMPARE(message.type, PageMessageType::Unknown);
}

void TestPreviewProtocol::decodesNonObjectJsonAsUnknown()
{
    const auto message = decodePageMessage(QStringLiteral("[1, 2, 3]"));
    QCOMPARE(message.type, PageMessageType::Unknown);
}

QTEST_APPLESS_MAIN(TestPreviewProtocol)
#include "test_preview_protocol.moc"
