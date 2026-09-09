#pragma once

#include <QPointer>

#include "preview/PreviewBackend.h"

class QWebEngineView;

namespace hungryeditor {

/// PreviewBackend backed by Chromium via Qt WebEngine. Gives the preview full
/// CSS and JavaScript, which later phases need for mermaid, KaTeX and the
/// scroll-sync bridge.
class QtWebEnginePreview : public PreviewBackend
{
    Q_OBJECT

public:
    explicit QtWebEnginePreview(QObject* parent = nullptr);
    ~QtWebEnginePreview() override;

    QWidget* widget() override;
    void setHtml(const QString& html, const QUrl& baseUrl = QUrl()) override;
    void runJavaScript(const QString& script,
                       std::function<void(const QVariant&)> callback = {}) override;

private:
    QPointer<QWebEngineView> view_;
};

} // namespace hungryeditor
