#include "preview/PreviewBridge.h"

namespace hungryeditor {

void PreviewBridge::setContent(const QString& html)
{
    if (html == content_) {
        return;
    }
    content_ = html;
    emit contentChanged(content_);
}

void PreviewBridge::setThemeCss(const QString& css)
{
    if (css == themeCss_) {
        return;
    }
    themeCss_ = css;
    emit themeCssChanged(themeCss_);
}

} // namespace hungryeditor
