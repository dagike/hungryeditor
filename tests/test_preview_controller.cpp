// Coverage for the debounce-and-render controller between editor and preview.

#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

#include "preview/PreviewBackend.h"
#include "preview/PreviewController.h"

using hungryeditor::PreviewBackend;
using hungryeditor::PreviewController;

namespace {

/// Records what the controller pushes, without a browser.
class FakeBackend : public PreviewBackend
{
public:
    QWidget* widget() override { return nullptr; }
    void setHtml(const QString&, const QUrl&) override {}
    void setContent(const QString& bodyHtml, const QUrl&) override { pushes << bodyHtml; }
    void setThemeCss(const QString&) override {}
    void runJavaScript(const QString&, const std::function<void(const QVariant&)>&) override {}
    void scrollToSourceLine(int) override {}
    bool print(QPrinter*) override { return false; }
    bool printToPdf(const QString&) override { return false; }

    QStringList pushes;
};

} // namespace

class TestPreviewController : public QObject
{
    Q_OBJECT

private slots:
    void coalescesRapidEdits();
    void flushRendersImmediately();
    void flushWithoutAPendingEditDoesNothing();
    void rendersMarkdownToHtmlFragment();
    void inlinesLocalImagesRelativeToTheDocument();
};

void TestPreviewController::coalescesRapidEdits()
{
    FakeBackend backend;
    PreviewController controller(&backend);
    controller.setDebounceInterval(30);
    QSignalSpy rendered(&controller, &PreviewController::rendered);

    const QStringList edits{QStringLiteral("# a"), QStringLiteral("# ab"), QStringLiteral("# abc")};
    for (const QString& text : edits) {
        controller.setMarkdown(text);
    }

    QVERIFY(rendered.wait(1000));
    QCOMPARE(rendered.count(), 1);
    QCOMPARE(backend.pushes.size(), 1);
    QVERIFY(backend.pushes.first().contains(QStringLiteral(">abc</h1>")));
}

void TestPreviewController::flushRendersImmediately()
{
    FakeBackend backend;
    PreviewController controller(&backend);
    controller.setDebounceInterval(5000); // long enough that only flush can fire it

    controller.setMarkdown(QStringLiteral("plain text\n"));
    controller.flush();

    QCOMPARE(backend.pushes.size(), 1);
    QVERIFY(
        backend.pushes.first().contains(QStringLiteral("<p data-src-line=\"0\">plain text</p>")));
}

void TestPreviewController::flushWithoutAPendingEditDoesNothing()
{
    FakeBackend backend;
    PreviewController controller(&backend);
    controller.flush();
    QVERIFY(backend.pushes.isEmpty());
}

void TestPreviewController::rendersMarkdownToHtmlFragment()
{
    FakeBackend backend;
    PreviewController controller(&backend);
    QSignalSpy rendered(&controller, &PreviewController::rendered);

    controller.setMarkdown(QStringLiteral("# Heading\n"));
    controller.flush();

    QCOMPARE(rendered.count(), 1);
    QVERIFY(rendered.first().at(0).toString().contains(QStringLiteral("<h1 data-src-line=\"0\">")));
}

void TestPreviewController::inlinesLocalImagesRelativeToTheDocument()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile png(dir.filePath(QStringLiteral("shot.png")));
    QVERIFY(png.open(QIODevice::WriteOnly));
    png.write(QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4"
                                     "2mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="));
    png.close();

    FakeBackend backend;
    PreviewController controller(&backend);
    controller.setDocumentPath(dir.filePath(QStringLiteral("note.md")));
    controller.setMarkdown(QStringLiteral("![shot](shot.png)\n"));
    controller.flush();

    QCOMPARE(backend.pushes.size(), 1);
    QVERIFY(backend.pushes.first().contains(QStringLiteral("src=\"data:image/png;base64,")));
}

QTEST_GUILESS_MAIN(TestPreviewController)
#include "test_preview_controller.moc"
