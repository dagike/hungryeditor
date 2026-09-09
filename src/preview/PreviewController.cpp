#include "preview/PreviewController.h"

#include "preview/PreviewBackend.h"

namespace hungryeditor {

PreviewController::PreviewController(PreviewBackend* backend, QObject* parent)
    : QObject(parent), backend_(backend)
{
    timer_.setSingleShot(true);
    timer_.setInterval(50);
    connect(&timer_, &QTimer::timeout, this, &PreviewController::render);
}

void PreviewController::setDebounceInterval(int milliseconds)
{
    timer_.setInterval(milliseconds);
}

int PreviewController::debounceInterval() const
{
    return timer_.interval();
}

void PreviewController::setMarkdown(const QString& markdown)
{
    pending_ = markdown;
    dirty_ = true;
    timer_.start();
}

void PreviewController::flush()
{
    if (!dirty_) {
        return;
    }
    timer_.stop();
    render();
}

void PreviewController::render()
{
    if (!dirty_) {
        return;
    }
    dirty_ = false;

    const QString html = renderer_.toHtml(pending_);
    if (backend_ != nullptr) {
        backend_->setContent(html);
    }
    emit rendered(html);
}

} // namespace hungryeditor
