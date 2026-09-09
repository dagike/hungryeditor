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

} // namespace hungryeditor
