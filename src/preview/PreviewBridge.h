#pragma once

#include <QObject>
#include <QString>

namespace hungryeditor {

/// The object the preview page talks to over QWebChannel. The host pushes
/// rendered HTML through `content`; the page reports back when it is wired up
/// and, later, scroll position and heading clicks.
class PreviewBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString content READ content NOTIFY contentChanged)

public:
    explicit PreviewBridge(QObject* parent = nullptr) : QObject(parent) {}

    QString content() const { return content_; }

    /// Push a new rendered body to the page. A no-op if it is unchanged.
    void setContent(const QString& html);

signals:
    void contentChanged(const QString& html);

    /// The page's script has connected and applied the initial content.
    void pageReady();

public slots:
    /// Called from the page once its QWebChannel handshake completes.
    void notifyReady() { emit pageReady(); }

private:
    QString content_;
};

} // namespace hungryeditor
