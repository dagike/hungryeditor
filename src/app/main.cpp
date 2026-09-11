// hungryeditor — a fast native Markdown editor.

#include <QApplication>
#include <QCommandLineParser>

#include "app/CommandLine.h"
#include "app/MainWindow.h"
#include "app/StatePaths.h"
#include "diagnostics/CrashReporter.h"
#include "io/Preferences.h"

#ifndef HUNGRYEDITOR_VERSION
#define HUNGRYEDITOR_VERSION "0.0.0"
#endif

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("hungryeditor"));
    QApplication::setApplicationVersion(QStringLiteral(HUNGRYEDITOR_VERSION));
    QApplication::setOrganizationName(QStringLiteral("hungryeditor"));

    QCommandLineParser parser;
    hungryeditor::configureCommandLineParser(parser);
    parser.process(app);

    // As early as practical, so as much of the app's runtime as possible is
    // covered. Read directly rather than through a MainWindow, which does
    // not exist yet.
    const QString stateDir = hungryeditor::defaultStateDirectory();
    const hungryeditor::Preferences preferences =
        hungryeditor::PreferencesStore(stateDir + QLatin1String("/preferences.json")).load();
    if (preferences.crashReportingEnabled) {
        hungryeditor::crashreporter::install(stateDir);
    }

    hungryeditor::MainWindow window;
    window.openFiles(hungryeditor::filesFromCommandLine(parser));
    window.restoreLastSession();
    window.show();

    return QApplication::exec();
}
