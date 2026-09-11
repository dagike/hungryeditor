#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

class QPrinter;
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

    /// Replace the whole document. `baseUrl` resolves relative links and images
    /// and is normally the directory of the file being edited. This reloads the
    /// page; use setContent() for live editing.
    virtual void setHtml(const QString& html, const QUrl& baseUrl = QUrl()) = 0;

    /// Swap just the rendered body, keeping the page (and its scroll position)
    /// in place. The first call brings the preview shell up; later calls stream
    /// `bodyHtml` in over the bridge. `baseUrl` resolves relative asset paths.
    virtual void setContent(const QString& bodyHtml, const QUrl& baseUrl = QUrl()) = 0;

    /// Install the preview stylesheet (from Theme::previewCss()). Applied live,
    /// without a reload; safe to call before the shell is up.
    virtual void setThemeCss(const QString& css) = 0;

    /// Evaluate `script` in the page. When `callback` is given it receives the
    /// result once available (an invalid QVariant if the script returned
    /// nothing or the page is gone).
    virtual void runJavaScript(const QString& script,
                               const std::function<void(const QVariant&)>& callback = {}) = 0;

    /// Scroll the preview so the block that came from source line `line` (or the
    /// next one after it) sits at the top of the viewport.
    virtual void scrollToSourceLine(int line) = 0;

    /// Render the current page onto `printer` — a physical printer, or one
    /// pointed at a file (QPrinter::PdfFormat). Blocks until the render
    /// engine finishes; returns whether it succeeded.
    virtual bool print(QPrinter* printer) = 0;

    /// Render the current page to a standalone PDF at `filePath`. Blocks
    /// until the render engine finishes; returns whether it succeeded.
    virtual bool printToPdf(const QString& filePath) = 0;

signals:
    /// Emitted once a setHtml() load settles; `ok` is false on a load error.
    void loadFinished(bool ok);

    /// Emitted once the preview shell is up and the bridge is connected, so
    /// the first setContent() has somewhere to land.
    void ready();

    /// The viewer scrolled the preview; `line` is the source line of the block
    /// now at the top. Not emitted for scrollToSourceLine()'s own movement.
    void scrolledToSourceLine(int line);

    /// The viewer clicked a heading in the preview; `line` is its source line.
    void clickedSourceLine(int line);

    /// The viewer toggled a task-list checkbox; `line` is the source line of its
    /// list item and `checked` is the checkbox's new state.
    void taskToggled(int line, bool checked);
};

} // namespace hungryeditor
