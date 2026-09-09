#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

class QWidget;

namespace hungryeditor {

/// The HTML preview surface, abstracted so the rendering engine can be swapped.
///
/// QtWebEnginePreview is the only implementation today; a WebView2 backend is
/// planned for the Windows installer (it cuts ~120 MB), so nothing on this
/// interface may name a Qt WebEngine type.
class PreviewBackend : public QObject
{
    Q_OBJECT

public:
    explicit PreviewBackend(QObject* parent = nullptr) : QObject(parent) {}

    /// The widget to embed in the window. The backend owns it and outlives it.
    virtual QWidget* widget() = 0;

    /// Replace the shown document. `baseUrl` resolves relative links and images
    /// and is normally the directory of the file being edited.
    virtual void setHtml(const QString& html, const QUrl& baseUrl = QUrl()) = 0;

    /// Evaluate `script` in the page. When `callback` is given it receives the
    /// result once available (an invalid QVariant if the script returned
    /// nothing or the page is gone).
    virtual void runJavaScript(const QString& script,
                               std::function<void(const QVariant&)> callback = {}) = 0;

signals:
    /// Emitted once a setHtml() load settles; `ok` is false on a load error.
    void loadFinished(bool ok);
};

} // namespace hungryeditor
