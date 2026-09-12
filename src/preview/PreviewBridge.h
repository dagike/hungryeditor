#pragma once

#include <QObject>
#include <QString>

namespace hungryeditor {

/// The object the preview page talks to over a single bidirectional string
/// channel (see PreviewProtocol.h for the wire format). Deliberately reduced
/// to one slot and one signal rather than QWebChannel's usual properties and
/// typed slots: a future WebView2 host has no equivalent of either, only a
/// postMessage(string)/message-event pair, so this is the intersection both
/// hosts can implement identically.
class PreviewBridge : public QObject
{
    Q_OBJECT

public:
    explicit PreviewBridge(QObject* parent = nullptr) : QObject(parent) {}

    /// Push a new rendered body to the page. A no-op if it is unchanged.
    void setContent(const QString& html);

    /// Push a new preview stylesheet to the page. A no-op if it is unchanged.
    void setThemeCss(const QString& css);

    /// Ask the page to scroll the block from source line `line` to the top.
    void requestScrollToLine(int line);

signals:
    /// A message for the page to apply, encoded per PreviewProtocol.h.
    void messageForPage(const QString& json);

    /// The page's transport connected, sent its "hello" and (in response to
    /// that) applied the current theme and content.
    void pageReady();

    /// The viewer scrolled the page; `line` is the source line now at the top.
    void viewerScrolled(int line);

    /// The viewer clicked a heading; `line` is its source line.
    void headingClicked(int line);

    /// The viewer toggled a task-list checkbox; `line` is the source line of its
    /// list item and `checked` is the checkbox's new state.
    void taskToggled(int line, bool checked);

public slots:
    /// Called with every message the page sends up the channel. Dispatches
    /// on its decoded type; an unrecognised message is silently ignored.
    void postMessage(const QString& json);

private:
    QString content_;
    QString themeCss_;
};

} // namespace hungryeditor
