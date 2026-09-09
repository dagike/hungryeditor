#pragma once

#include <QPointer>
#include <QUrl>

#include "preview/PreviewBackend.h"

class QWebChannel;
class QWebEngineView;

namespace hungryeditor {

class PreviewBridge;

/// PreviewBackend backed by Chromium via Qt WebEngine. Gives the preview full
/// CSS and JavaScript, which later phases need for mermaid, KaTeX and the
/// scroll-sync bridge.
///
/// Live edits go through setContent(): the first call loads a fixed shell page
/// that connects a QWebChannel back to a PreviewBridge, and every call after
/// that streams the rendered body in without a reload.
class QtWebEnginePreview : public PreviewBackend
{
    Q_OBJECT

public:
    explicit QtWebEnginePreview(QObject* parent = nullptr);
    ~QtWebEnginePreview() override;

    QWidget* widget() override;
    void setHtml(const QString& html, const QUrl& baseUrl = QUrl()) override;
    void setContent(const QString& bodyHtml, const QUrl& baseUrl = QUrl()) override;
    void runJavaScript(const QString& script,
                       std::function<void(const QVariant&)> callback = {}) override;

private:
    QPointer<QWebEngineView> view_;
    PreviewBridge* bridge_;
    QWebChannel* channel_;
    QUrl baseUrl_;
    bool shellRequested_ = false;
};

} // namespace hungryeditor
