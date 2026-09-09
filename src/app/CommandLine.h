#pragma once

#include <QStringList>

class QCommandLineParser;

namespace hungryeditor {

/// Configure `parser` with hungryeditor's command-line interface: the help and
/// version options plus a positional `[files...]` argument.
void configureCommandLineParser(QCommandLineParser& parser);

/// The positional file arguments held by `parser`, resolved to absolute paths
/// with duplicates removed. Call after the parser has parsed the arguments.
QStringList filesFromCommandLine(const QCommandLineParser& parser);

} // namespace hungryeditor
