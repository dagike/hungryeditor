#pragma once

#include <QObject>
#include <QString>

namespace hungryeditor {

/// The object the preview page talks to over QWebChannel. The host pushes
/// rendered HTML and scroll requests down; the page reports back when it is
/// wired up and when the viewer scrolls it.
class PreviewBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString content READ content NOTIFY contentChanged)
    Q_PROPERTY(QString themeCss READ themeCss NOTIFY themeCssChanged)

public:
    explicit PreviewBridge(QObject* parent = nullptr) : QObject(parent) {}

    QString content() const { return content_; }
    QString themeCss() const { return themeCss_; }

    /// Push a new rendered body to the page. A no-op if it is unchanged.
    void setContent(const QString& html);

    /// Push a new preview stylesheet to the page. A no-op if it is unchanged.
    void setThemeCss(const QString& css);

    /// Ask the page to scroll the block from source line `line` to the top.
    void requestScrollToLine(int line) { emit scrollToLineRequested(line); }

signals:
    void contentChanged(const QString& html);
    void themeCssChanged(const QString& css);

    /// Host wants the page scrolled so `line`'s block is at the top.
    void scrollToLineRequested(int line);

    /// The page's script has connected and applied the initial content.
    void pageReady();

    /// The viewer scrolled the page; `line` is the source line now at the top.
    void viewerScrolled(int line);

public slots:
    /// Called from the page once its QWebChannel handshake completes.
    void notifyReady() { emit pageReady(); }

    /// Called from the page's scroll handler.
    void reportScroll(int line) { emit viewerScrolled(line); }

private:
    QString content_;
    QString themeCss_;
};

} // namespace hungryeditor
