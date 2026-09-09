// hungryeditor — a fast native Markdown editor.

#include <QApplication>
#include <QCommandLineParser>

#include "app/CommandLine.h"
#include "app/MainWindow.h"

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

    hungryeditor::MainWindow window;
    window.openFiles(hungryeditor::filesFromCommandLine(parser));
    window.restoreUnsavedFromLastSession();
    window.show();

    return QApplication::exec();
}
