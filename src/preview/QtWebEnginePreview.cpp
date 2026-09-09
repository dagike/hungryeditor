#include "preview/QtWebEnginePreview.h"

#include <utility>

#include <QWebEnginePage>
#include <QWebEngineView>

namespace hungryeditor {

QtWebEnginePreview::QtWebEnginePreview(QObject* parent)
    : PreviewBackend(parent), view_(new QWebEngineView)
{
    connect(view_.data(), &QWebEngineView::loadFinished, this, &PreviewBackend::loadFinished);
}

QtWebEnginePreview::~QtWebEnginePreview()
{
    // QPointer is null if the view was embedded and destroyed with its parent.
    delete view_.data();
}

QWidget* QtWebEnginePreview::widget()
{
    return view_.data();
}

void QtWebEnginePreview::setHtml(const QString& html, const QUrl& baseUrl)
{
    view_->setHtml(html, baseUrl);
}

void QtWebEnginePreview::runJavaScript(const QString& script,
                                      std::function<void(const QVariant&)> callback)
{
    if (callback) {
        view_->page()->runJavaScript(script, std::move(callback));
    } else {
        view_->page()->runJavaScript(script);
    }
}

} // namespace hungryeditor
