#include "preview/PreviewBridge.h"

#include "preview/PreviewProtocol.h"

namespace hungryeditor {

using preview_protocol::decodePageMessage;
using preview_protocol::PageMessage;
using preview_protocol::PageMessageType;

void PreviewBridge::setContent(const QString& html)
{
    if (html == content_) {
        return;
    }
    content_ = html;
    emit messageForPage(preview_protocol::encodeContent(content_));
}

void PreviewBridge::setThemeCss(const QString& css)
{
    if (css == themeCss_) {
        return;
    }
    themeCss_ = css;
    emit messageForPage(preview_protocol::encodeTheme(themeCss_));
}

void PreviewBridge::requestScrollToLine(int line)
{
    emit messageForPage(preview_protocol::encodeScrollToLine(line));
}

void PreviewBridge::postMessage(const QString& json)
{
    const PageMessage message = decodePageMessage(json);
    switch (message.type) {
    case PageMessageType::Hello:
        // No channel properties to read, unlike QWebChannel: push the
        // current state down instead, in the same order the old code path
        // applied it (theme first, so content lands on a styled page).
        emit messageForPage(preview_protocol::encodeTheme(themeCss_));
        emit messageForPage(preview_protocol::encodeContent(content_));
        break;
    case PageMessageType::Ready:
        emit pageReady();
        break;
    case PageMessageType::Scroll:
        emit viewerScrolled(message.line);
        break;
    case PageMessageType::HeadingClick:
        emit headingClicked(message.line);
        break;
    case PageMessageType::TaskToggle:
        emit taskToggled(message.line, message.checked);
        break;
    case PageMessageType::Unknown:
        break;
    }
}

} // namespace hungryeditor
