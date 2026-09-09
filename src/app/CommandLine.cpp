#include "app/CommandLine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>

namespace hungryeditor {

void configureCommandLineParser(QCommandLineParser& parser)
{
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "A fast, native Markdown editor."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("files"),
        QCoreApplication::translate("main", "Markdown or text files to open."),
        QStringLiteral("[files...]"));
}

QStringList filesFromCommandLine(const QCommandLineParser& parser)
{
    QStringList resolved;
    for (const QString& argument : parser.positionalArguments()) {
        const QString absolute = QFileInfo(argument).absoluteFilePath();
        if (!absolute.isEmpty() && !resolved.contains(absolute)) {
            resolved.append(absolute);
        }
    }
    return resolved;
}

} // namespace hungryeditor
