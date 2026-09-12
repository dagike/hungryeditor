// Capture-viability probe for the demo recorder (tests/helpers/demo_recorder.cpp).
//
// QWebEngineView's render widget is a QQuickWidget internally. QWidget::grab()
// runs the ordinary render()/paintEvent path, which the default RHI/OpenGL Qt
// Quick backend does not participate in -- it composites via a separate
// texture path grab() never sees. So a plain window.grab() risks capturing a
// blank preview pane under an offscreen QPA platform. QT_QUICK_BACKEND=software
// should make QQuickWidget rasterize into a QImage and paint it in paintEvent,
// which *does* show up in grab() -- but that needs to be confirmed on this
// machine/Qt version, not assumed. This probe answers that empirically.
//
// Usage: demo_probe <out-dir> <tag>
//
// Run it under each candidate environment (see tests/helpers/record_demo.sh's
// header comment for the exact invocations) and read the PASS/FAIL(flat)
// verdict lines. Re-run this whenever Qt is upgraded and demo clips suddenly
// come out with a blank preview pane.

#include <cmath>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QQuickWidget>
#include <QSet>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

#include "app/MainWindow.h"
#include "editor/Editor.h"
#include "preview/PreviewBackend.h"
#include "preview/PreviewController.h"

namespace {

QString probeDocument()
{
    return QStringLiteral(R"(# Capture probe

A paragraph, so there is ordinary text on the page too.

```mermaid
graph TD;
A[Start] --> B[Render];
B --> C[Grab];
```

$$E = mc^2$$
)");
}

/// Distinct-colour count (sampled) and luminance stddev for `image`. A blank
/// or solid-fill rectangle scores ~1-3 colours and a stddev near 0; real
/// rendered content scores thousands of colours and a stddev well above 8.
struct ImageStats
{
    int distinctColors = 0;
    double luminanceStdDev = 0.0;
};

ImageStats analyze(const QImage& image)
{
    if (image.isNull()) {
        return {};
    }
    const QImage rgb = image.convertToFormat(QImage::Format_RGB32);

    QSet<QRgb> colors;
    double sum = 0.0;
    double sumSquares = 0.0;
    qint64 sampleCount = 0;

    for (int y = 0; y < rgb.height(); y += 4) {
        const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < rgb.width(); x += 4) {
            const QRgb pixel = line[x];
            if (colors.size() < 5000) {
                colors.insert(pixel);
            }
            const double luminance =
                0.299 * qRed(pixel) + 0.587 * qGreen(pixel) + 0.114 * qBlue(pixel);
            sum += luminance;
            sumSquares += luminance * luminance;
            ++sampleCount;
        }
    }

    if (sampleCount == 0) {
        return {};
    }
    const double mean = sum / static_cast<double>(sampleCount);
    const double variance = sumSquares / static_cast<double>(sampleCount) - mean * mean;
    return {static_cast<int>(colors.size()), std::sqrt(std::max(0.0, variance))};
}

void saveAndReport(const QImage& image, const QString& path, const QString& label)
{
    if (image.isNull()) {
        qInfo().noquote() << label << ": SKIPPED (nothing to grab)";
        return;
    }
    image.save(path, "PNG");
    const ImageStats stats = analyze(image);
    const bool pass = stats.distinctColors > 200 && stats.luminanceStdDev > 8.0;
    qInfo().noquote() << label << ":" << (pass ? "PASS" : "FAIL(flat)")
                      << "colors=" << stats.distinctColors
                      << "stddev=" << QString::number(stats.luminanceStdDev, 'f', 2)
                      << "size=" << image.size() << "->" << path;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    if (argc < 3) {
        qWarning() << "usage: demo_probe <out-dir> <tag>";
        return 2;
    }
    const QString outDir = QString::fromLocal8Bit(argv[1]);
    const QString tag = QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(outDir);

    qInfo().noquote() << "platform:" << QGuiApplication::platformName();

    QTemporaryDir state;
    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    window.resize(1280, 800);
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
        qWarning() << "window never exposed";
        return 1;
    }

    QAction* splitView = window.findChild<QAction*>(QStringLiteral("action.viewSplit"));
    if (splitView == nullptr) {
        qWarning() << "action.viewSplit not found -- has it been renamed?";
        return 1;
    }
    splitView->trigger();

    QSignalSpy ready(window.previewBackend(), &hungryeditor::PreviewBackend::ready);
    window.editor()->setText(probeDocument());
    if (!ready.wait(20000)) {
        qWarning() << "preview never became ready";
    }

    QSignalSpy rendered(window.previewController(), &hungryeditor::PreviewController::rendered);
    rendered.wait(4000);
    QTest::qWait(4000); // settle time for mermaid/KaTeX's own async render

    saveAndReport(window.grab().toImage(), outDir + QStringLiteral("/probe-%1-window.png").arg(tag),
                  QStringLiteral("window.grab()"));
    saveAndReport(window.previewWidget()->grab().toImage(),
                  outDir + QStringLiteral("/probe-%1-previewwidget.png").arg(tag),
                  QStringLiteral("previewWidget()->grab()"));

    auto* quick = window.previewWidget()->findChild<QQuickWidget*>();
    if (quick != nullptr) {
        saveAndReport(quick->grabFramebuffer(),
                      outDir + QStringLiteral("/probe-%1-quickfb.png").arg(tag),
                      QStringLiteral("QQuickWidget::grabFramebuffer()"));
    } else {
        qInfo() << "no QQuickWidget found under previewWidget()";
    }

    return 0;
}
