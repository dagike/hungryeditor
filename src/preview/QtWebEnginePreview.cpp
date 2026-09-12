#include "preview/QtWebEnginePreview.h"

#include <QEventLoop>
#include <QFile>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineView>

#include "preview/PreviewBridge.h"

namespace hungryeditor {

namespace {

// The preview shell: loaded once, then setContent() streams the rendered body
// into #hungryeditor-content over the QWebChannel. A qrc: base URL lets the
// page pull Qt WebEngine's bundled qwebchannel.js.
const QUrl kShellBaseUrl(QStringLiteral("qrc:/hungryeditor/preview/"));

// The preview shell page, lifted verbatim into src/preview/shell/shell.html
// so a future preview backend can load the identical file rather than fork
// the mermaid/KaTeX lazy-render logic, error surfaces and scroll-sync math
// it contains. @HE_ORIGIN@ is the file's one substitution point (see its own
// CSP comment); Qt WebEngine serves everything under the qrc: scheme, so
// that is what gets substituted back in here.
QString loadShellHtml()
{
    QFile file(QStringLiteral(":/hungryeditor/preview/shell.html"));
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString html = QString::fromUtf8(file.readAll());
    html.replace(QLatin1String("@HE_ORIGIN@"), QLatin1String("qrc:"));
    return html;
}

} // namespace

QtWebEnginePreview::QtWebEnginePreview(QObject* parent)
    : PreviewBackend(parent), view_(std::make_unique<QWebEngineView>()),
      bridge_(new PreviewBridge(this)), channel_(new QWebChannel(this))
{
    channel_->registerObject(QStringLiteral("bridge"), bridge_);
    view_->page()->setWebChannel(channel_);

    connect(view_.get(), &QWebEngineView::loadFinished, this, &PreviewBackend::loadFinished);
    connect(bridge_, &PreviewBridge::pageReady, this, &PreviewBackend::ready);
    connect(bridge_, &PreviewBridge::viewerScrolled, this, &PreviewBackend::scrolledToSourceLine);
    connect(bridge_, &PreviewBridge::headingClicked, this, &PreviewBackend::clickedSourceLine);
    connect(bridge_, &PreviewBridge::taskToggled, this, &PreviewBackend::taskToggled);
}

QtWebEnginePreview::~QtWebEnginePreview() = default;

QWidget* QtWebEnginePreview::widget()
{
    return view_.get();
}

void QtWebEnginePreview::setHtml(const QString& html, const QUrl& baseUrl)
{
    shellRequested_ = false; // a fresh full document replaces the shell
    view_->setHtml(html, baseUrl);
}

void QtWebEnginePreview::setContent(const QString& bodyHtml, const QUrl& baseUrl)
{
    baseUrl_ = baseUrl;
    bridge_->setContent(bodyHtml); // seed before the shell's script reads it

    if (!shellRequested_) {
        shellRequested_ = true;
        view_->setHtml(loadShellHtml(), kShellBaseUrl);
    }
}

void QtWebEnginePreview::runJavaScript(const QString& script,
                                       const std::function<void(const QVariant&)>& callback)
{
    if (callback) {
        view_->page()->runJavaScript(script, callback);
    } else {
        view_->page()->runJavaScript(script);
    }
}

void QtWebEnginePreview::setThemeCss(const QString& css)
{
    bridge_->setThemeCss(css);
}

void QtWebEnginePreview::scrollToSourceLine(int line)
{
    bridge_->requestScrollToLine(line);
}

bool QtWebEnginePreview::print(QPrinter* printer)
{
    QEventLoop loop;
    bool result = false;
    const QMetaObject::Connection connection =
        connect(view_.get(), &QWebEngineView::printFinished, &loop, [&](bool ok) {
            result = ok;
            loop.quit();
        });
    view_->print(printer);
    loop.exec();
    QObject::disconnect(connection);
    return result;
}

bool QtWebEnginePreview::printToPdf(const QString& filePath)
{
    QEventLoop loop;
    bool result = false;
    const QMetaObject::Connection connection = connect(
        view_.get(), &QWebEngineView::pdfPrintingFinished, &loop, [&](const QString&, bool ok) {
            result = ok;
            loop.quit();
        });
    view_->printToPdf(filePath);
    loop.exec();
    QObject::disconnect(connection);
    return result;
}

} // namespace hungryeditor
