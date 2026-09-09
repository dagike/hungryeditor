// Coverage for command-line parsing of file arguments.

#include <QCommandLineParser>
#include <QFileInfo>
#include <QtTest>

#include "app/CommandLine.h"

using hungryeditor::configureCommandLineParser;
using hungryeditor::filesFromCommandLine;

namespace {

QStringList parseFiles(const QStringList& arguments)
{
    QCommandLineParser parser;
    configureCommandLineParser(parser);
    if (!parser.parse(arguments)) {
        return {};
    }
    return filesFromCommandLine(parser);
}

} // namespace

class TestCommandLine : public QObject
{
    Q_OBJECT

private slots:
    void noArgumentsYieldNoFiles();
    void positionalArgumentsBecomeAbsolutePaths();
    void relativePathsResolveAgainstTheWorkingDirectory();
    void repeatedPathsAreDeduplicated();
    void helpAndVersionOptionsAreRecognised();
    void unknownOptionsFailToParse();
};

void TestCommandLine::noArgumentsYieldNoFiles()
{
    QVERIFY(parseFiles({QStringLiteral("hungryeditor")}).isEmpty());
}

void TestCommandLine::positionalArgumentsBecomeAbsolutePaths()
{
    const QStringList files = parseFiles(
        {QStringLiteral("hungryeditor"), QStringLiteral("a.md"), QStringLiteral("b.md")});
    QCOMPARE(files.size(), 2);
    QVERIFY(QFileInfo(files.at(0)).isAbsolute());
    QVERIFY(files.at(0).endsWith(QStringLiteral("a.md")));
    QVERIFY(files.at(1).endsWith(QStringLiteral("b.md")));
}

void TestCommandLine::relativePathsResolveAgainstTheWorkingDirectory()
{
    const QStringList files =
        parseFiles({QStringLiteral("hungryeditor"), QStringLiteral("notes/todo.md")});
    QCOMPARE(files.size(), 1);
    QCOMPARE(QFileInfo(files.at(0)).fileName(), QStringLiteral("todo.md"));
    QCOMPARE(files.at(0), QFileInfo(QStringLiteral("notes/todo.md")).absoluteFilePath());
}

void TestCommandLine::repeatedPathsAreDeduplicated()
{
    const QStringList files = parseFiles(
        {QStringLiteral("hungryeditor"), QStringLiteral("dup.md"), QStringLiteral("dup.md")});
    QCOMPARE(files.size(), 1);
}

void TestCommandLine::helpAndVersionOptionsAreRecognised()
{
    QCommandLineParser parser;
    configureCommandLineParser(parser);

    QVERIFY(parser.parse({QStringLiteral("hungryeditor"), QStringLiteral("--help")}));
    QVERIFY(parser.isSet(QStringLiteral("help")));

    QCommandLineParser versionParser;
    configureCommandLineParser(versionParser);
    QVERIFY(versionParser.parse({QStringLiteral("hungryeditor"), QStringLiteral("--version")}));
    QVERIFY(versionParser.isSet(QStringLiteral("version")));
}

void TestCommandLine::unknownOptionsFailToParse()
{
    QCommandLineParser parser;
    configureCommandLineParser(parser);
    QVERIFY(!parser.parse({QStringLiteral("hungryeditor"), QStringLiteral("--nonsense")}));
}

QTEST_MAIN(TestCommandLine)
#include "test_command_line.moc"
