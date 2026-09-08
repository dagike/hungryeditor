// Smoke test for the application window. Runs headless via the offscreen
// platform plugin. Replaced/expanded by proper Qt Test cases from commit 0.6.

#include <cstdio>

#include <QAction>
#include <QApplication>
#include <QMenuBar>

#include "app/MainWindow.h"

#include <ScintillaEditBase.h>

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    hungryeditor::MainWindow window;

    if (window.centralWidget() != window.editor() || window.editor() == nullptr) {
        std::fprintf(stderr, "central widget is not the editor\n");
        return 1;
    }

    if (window.menuBar()->actions().size() != 2) {
        std::fprintf(stderr, "expected 2 top-level menus, got %lld\n",
                     static_cast<long long>(window.menuBar()->actions().size()));
        return 1;
    }

    for (const char* name : {"action.quit", "action.about"}) {
        if (window.findChild<QAction*>(QString::fromLatin1(name)) == nullptr) {
            std::fprintf(stderr, "missing action \"%s\"\n", name);
            return 1;
        }
    }

    window.show();
    app.processEvents();

    std::printf("ok: main window built with editor and menus\n");
    return 0;
}
