#pragma once

#include <memory>

#include <QUrl>

#include "preview/PreviewBackend.h"

class QWebChannel;
class QWebEngineView;

namespace hungryeditor {

class PreviewBridge;

/// PreviewBackend backed by Chromium via Qt WebEngine. Gives the preview full
/// CSS and JavaScript, which mermaid diagrams, KaTeX math and the scroll-sync
/// bridge all rely on.
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
    void setThemeCss(const QString& css) override;
    void runJavaScript(const QString& script,
                       const std::function<void(const QVariant&)>& callback = {}) override;
    void scrollToSourceLine(int line) override;
    bool print(QPrinter* printer) override;
    bool printToPdf(const QString& filePath) override;

private:
    // Owned until widget() is embedded in a layout, which reparents it; the
    // backend must outlive whatever it is embedded in.
    std::unique_ptr<QWebEngineView> view_;
    PreviewBridge* bridge_;
    QWebChannel* channel_;
    QUrl baseUrl_;
    bool shellRequested_ = false;
};

} // namespace hungryeditor
