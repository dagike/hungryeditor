// Records a scripted "scene" of the real hungryeditor MainWindow, running
// under the offscreen QPA platform (see demo_probe.cpp, which confirmed
// QWidget::grab() captures the QtWebEngine preview correctly on this
// machine), as a sequence of numbered PNG frames. tests/helpers/record_demo.sh
// turns those into an MP4 and a GIF per scene for portfolio clips.
//
// Usage: demo_recorder --scene <name> --out <dir> [--fps N] [--width N]
//                       [--height N] [--max-frames N]
//        demo_recorder --list
//
// Never run by CTest -- like crash_helper, this is a manually-invoked dev
// tool, not a test.

#include <cstdio>

#include <QApplication>
#include <QBuffer>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QSplitter>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "app/MainWindow.h"
#include "editor/Editor.h"
#include "preview/PreviewBackend.h"

namespace {

void setEnvIfUnset(const char* name, const char* value)
{
    if (!qEnvironmentVariableIsSet(name)) {
        qputenv(name, value);
    }
}

/// Grabs `target_` onto a fixed-size canvas at a fixed cadence, driven by a
/// QTimer that keeps firing for as long as *any* nested event loop spins --
/// including a scene's own QTest::qWait()/QSignalSpy::wait() calls. That is
/// what lets a scripted action and the async preview render that follows it
/// both land on video without the scene having to think about frame timing.
///
/// The canvas is always exactly `canvas_` in size (composited via QPainter,
/// not a raw grab()) so a stray mid-scene resize can never hand ffmpeg a
/// variable frame size later. If a grab overruns its slot, the same PNG
/// bytes get written more than once ("catch-up") so wall-clock time and
/// video playback time stay aligned instead of the clip silently playing
/// fast.
class Recorder
{
public:
    Recorder(QWidget* target, QDir outDir, QSize canvas, int fps, int maxFrames)
        : target_(target), outDir_(std::move(outDir)), canvas_(canvas),
          intervalMs_(std::max(1, 1000 / std::max(1, fps))), maxFrames_(maxFrames)
    {
        outDir_.mkpath(QStringLiteral("."));
        QObject::connect(&timer_, &QTimer::timeout, &timer_, [this] { tick(); });
    }

    void start()
    {
        clock_.start();
        timer_.start(intervalMs_);
        tick();
    }

    void stop()
    {
        timer_.stop();
        tick();
    }

    int frameCount() const { return written_; }
    bool hitFrameCap() const { return written_ >= maxFrames_; }

private:
    QString framePath(int index) const
    {
        return outDir_.filePath(QStringLiteral("frame_%1.png").arg(index, 5, 10, QLatin1Char('0')));
    }

    QImage capture() const
    {
        QImage canvas(canvas_, QImage::Format_RGB32);
        canvas.fill(Qt::white);
        QPainter painter(&canvas);
        painter.drawImage(0, 0, target_->grab().toImage());
        return canvas;
    }

    void tick()
    {
        if (written_ >= maxFrames_) {
            return;
        }
        const int due = static_cast<int>(clock_.elapsed() / intervalMs_) + 1;
        if (written_ >= due) {
            return;
        }

        QByteArray png;
        {
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            capture().save(&buffer, "PNG", 40);
        }
        while (written_ < due && written_ < maxFrames_) {
            QFile file(framePath(++written_));
            file.open(QIODevice::WriteOnly);
            file.write(png);
        }
    }

    QWidget* target_;
    QDir outDir_;
    QSize canvas_;
    int intervalMs_;
    int maxFrames_;
    QTimer timer_;
    QElapsedTimer clock_;
    int written_ = 0;
};

/// The verbs a scene script is written against. Wraps this codebase's own
/// established test idioms for driving MainWindow (raw QKeyEvent +
/// QCoreApplication::sendEvent, findChild<QAction*>(name)->trigger() --
/// see tests/test_main_window.cpp and tests/test_editor.cpp, which never use
/// QTest::keyClick) rather than inventing new ones.
class Director
{
public:
    // previewReady_ must attach before MainWindow::show() ever pumps an
    // event loop: the constructor defaults to Split view and creates the
    // preview via a deferred QTimer::singleShot(0, ...), so a spy built
    // later (e.g. lazily inside waitForPreviewReady()) can easily attach
    // after ready() already fired and then hang for its full timeout.
    // previewBackend() forces creation immediately and deterministically
    // instead of waiting for that timer.
    Director(hungryeditor::MainWindow& window, Recorder& recorder)
        : window_(window), recorder_(recorder),
          previewReady_(window_.previewBackend(), &hungryeditor::PreviewBackend::ready)
    {
    }

    hungryeditor::MainWindow& window() const { return window_; }
    hungryeditor::Editor* editor() const { return window_.editor(); }

    /// A deliberate pause -- the recorder keeps grabbing frames throughout.
    void beat(int ms = 700) { QTest::qWait(ms); }

    /// Triggers a QAction by its objectName. Fails loudly (not silently) on
    /// a name that no longer exists, since a renamed action would otherwise
    /// just produce a boring, wrong clip instead of an obvious error.
    QAction* act(const char* actionObjectName)
    {
        QAction* action = window_.findChild<QAction*>(QString::fromLatin1(actionObjectName));
        if (action == nullptr) {
            qFatal("demo_recorder: action '%s' not found -- has it been renamed?",
                   actionObjectName);
        }
        action->trigger();
        beat(200);
        return action;
    }

    void key(QWidget* widget, Qt::Key k, Qt::KeyboardModifiers mods = Qt::NoModifier,
             int afterMs = 90)
    {
        QKeyEvent press(QEvent::KeyPress, k, mods);
        QCoreApplication::sendEvent(widget, &press);
        QKeyEvent release(QEvent::KeyRelease, k, mods);
        QCoreApplication::sendEvent(widget, &release);
        beat(afterMs);
    }

    /// Sends `text` into `widget` one character at a time -- what makes a
    /// clip look like someone is actually typing, instead of an instant wall
    /// of text appearing.
    void type(QWidget* widget, const QString& text, int msPerChar = 30)
    {
        for (const QChar ch : text) {
            const QString one(ch);
            QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, one);
            QCoreApplication::sendEvent(widget, &press);
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, one);
            QCoreApplication::sendEvent(widget, &release);
            beat(msPerChar);
        }
    }

    void typeInEditor(const QString& text, int msPerChar = 30) { type(editor(), text, msPerChar); }

    /// Seeds the whole document instantly (for content that doesn't need to
    /// be shown being typed -- typing an entire demo document would blow the
    /// clip's time budget).
    void loadDocument(const QString& markdown) { editor()->setText(markdown); }

    /// Waits for the preview shell's one-time "connected and applied initial
    /// content" signal, using the spy attached at construction time so it
    /// cannot miss an emission that happened before a scene got around to
    /// calling this.
    bool waitForPreviewReady(int timeoutMs = 20000)
    {
        if (previewReady_.count() > 0) {
            return true;
        }
        return previewReady_.wait(timeoutMs);
    }

    /// Polls the preview's JS context for `expr` to become truthy -- the
    /// only way to know mermaid/KaTeX actually finished painting, mirroring
    /// tests/test_preview.cpp's evalJs() polling helper. Never required for
    /// a scene to proceed; a timeout just means the clip moves on anyway.
    bool waitForJs(const QString& expr, int timeoutMs = 15000)
    {
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < timeoutMs) {
            bool done = false;
            bool truthy = false;
            window_.previewBackend()->runJavaScript(expr, [&](const QVariant& value) {
                truthy = value.toBool();
                done = true;
            });
            while (!done && clock.elapsed() < timeoutMs) {
                QTest::qWait(50);
            }
            if (truthy) {
                return true;
            }
            QTest::qWait(200);
        }
        return false;
    }

    /// The split-view preview starts pinned to its minimum width rather than
    /// an even split (QSplitter::addWidget() doesn't redistribute existing
    /// space on its own) -- fine for real use where a user just drags the
    /// divider, but a squished preview makes for a bad-looking clip.
    void balanceSplitter()
    {
        auto* splitter = window_.findChild<QSplitter*>();
        if (splitter == nullptr) {
            return;
        }
        const int total =
            splitter->orientation() == Qt::Horizontal ? splitter->width() : splitter->height();
        splitter->setSizes({total / 2, total - total / 2});
    }

private:
    hungryeditor::MainWindow& window_;
    Recorder& recorder_;
    QSignalSpy previewReady_;
};

using SceneFn = void (*)(Director&);

struct Scene
{
    const char* name;
    const char* description;
    SceneFn run;
};

void sceneSmoke(Director& director)
{
    director.act("action.viewSplit");
    director.balanceSplitter();
    director.waitForPreviewReady();
    director.loadDocument(
        QStringLiteral("# Smoke test\n\nJust checking the pipeline works end to end.\n"));
    director.beat(5000);
}

constexpr Scene kScenes[] = {
    {"smoke", "Minimal end-to-end pipeline check (split view, static content, a pause)",
     &sceneSmoke},
};

const Scene* findScene(const QString& name)
{
    for (const Scene& scene : kScenes) {
        if (name == QLatin1String(scene.name)) {
            return &scene;
        }
    }
    return nullptr;
}

void listScenes(FILE* stream)
{
    for (const Scene& scene : kScenes) {
        std::fprintf(stream, "  %-16s %s\n", scene.name, scene.description);
    }
}

} // namespace

int main(int argc, char** argv)
{
    // Must precede QApplication. Only fills in what the caller left unset,
    // so tests/helpers/record_demo.sh (or a one-off manual run) can override
    // any of them.
    setEnvIfUnset("QT_QPA_PLATFORM", "offscreen:width=1600;height=1000");
    setEnvIfUnset("QT_QUICK_BACKEND", "software");
    setEnvIfUnset("QTWEBENGINE_CHROMIUM_FLAGS",
                  "--no-sandbox --disable-gpu --disable-dev-shm-usage");
    setEnvIfUnset("QTWEBENGINE_DISABLE_SANDBOX", "1");
    setEnvIfUnset("QT_ENABLE_HIGHDPI_SCALING", "0");
    setEnvIfUnset("QT_SCALE_FACTOR", "1");
    setEnvIfUnset("QT_FONT_DPI", "96");

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("hungryeditor-demo"));
    QApplication::setOrganizationName(QStringLiteral("hungryeditor"));
    QApplication::setCursorFlashTime(0); // no caret-blink flicker inflating GIF size

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Records a scripted hungryeditor scene as numbered PNG frames."));
    parser.addHelpOption();
    const QCommandLineOption sceneOpt(QStringLiteral("scene"), QStringLiteral("Scene to record."),
                                      QStringLiteral("name"));
    const QCommandLineOption outOpt(QStringLiteral("out"),
                                    QStringLiteral("Output directory for frames."),
                                    QStringLiteral("dir"));
    const QCommandLineOption fpsOpt(QStringLiteral("fps"), QStringLiteral("Capture frame rate."),
                                    QStringLiteral("fps"), QStringLiteral("15"));
    const QCommandLineOption widthOpt(QStringLiteral("width"), QStringLiteral("Window width."),
                                      QStringLiteral("px"), QStringLiteral("1280"));
    const QCommandLineOption heightOpt(QStringLiteral("height"), QStringLiteral("Window height."),
                                       QStringLiteral("px"), QStringLiteral("800"));
    const QCommandLineOption maxFramesOpt(QStringLiteral("max-frames"),
                                          QStringLiteral("Hard cap on frame count."),
                                          QStringLiteral("n"), QStringLiteral("1500"));
    const QCommandLineOption listOpt(QStringLiteral("list"),
                                     QStringLiteral("List available scenes and exit."));
    parser.addOption(sceneOpt);
    parser.addOption(outOpt);
    parser.addOption(fpsOpt);
    parser.addOption(widthOpt);
    parser.addOption(heightOpt);
    parser.addOption(maxFramesOpt);
    parser.addOption(listOpt);
    parser.process(app);

    if (parser.isSet(listOpt)) {
        listScenes(stdout);
        return 0;
    }
    if (!parser.isSet(sceneOpt) || !parser.isSet(outOpt)) {
        std::fprintf(stderr,
                     "usage: demo_recorder --scene <name> --out <dir> [--fps N] [--width N] "
                     "[--height N] [--max-frames N]\navailable scenes:\n");
        listScenes(stderr);
        return 2;
    }

    const Scene* scene = findScene(parser.value(sceneOpt));
    if (scene == nullptr) {
        std::fprintf(stderr, "unknown scene '%s'\navailable scenes:\n",
                     qPrintable(parser.value(sceneOpt)));
        listScenes(stderr);
        return 2;
    }

    const int fps = parser.value(fpsOpt).toInt();
    const int width = parser.value(widthOpt).toInt();
    const int height = parser.value(heightOpt).toInt();
    const int maxFrames = parser.value(maxFramesOpt).toInt();

    QDir outDir(parser.value(outOpt));
    if (!outDir.exists() && !outDir.mkpath(QStringLiteral("."))) {
        std::fprintf(stderr, "could not create output directory '%s'\n",
                     qPrintable(outDir.absolutePath()));
        return 3;
    }

    QTemporaryDir state;
    hungryeditor::MainWindow window;
    window.setStateDirectory(state.path());
    window.resize(width, height);

    Recorder recorder(&window, outDir, QSize(width, height), fps, maxFrames);
    // Constructed before show(): Director's ctor attaches the preview-ready
    // spy immediately, before any event loop pump can run MainWindow's
    // deferred Split-view preview creation and its ready() signal.
    Director director(window, recorder);

    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
        std::fprintf(stderr, "window never exposed\n");
        return 1;
    }

    recorder.start();
    scene->run(director);
    recorder.stop();

    qInfo("demo_recorder: scene=%s frames=%d fps=%d size=%dx%d dir=%s", scene->name,
          recorder.frameCount(), fps, width, height, qPrintable(outDir.absolutePath()));

    return recorder.hitFrameCap() ? 4 : 0;
}
