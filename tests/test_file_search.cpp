// Coverage for the recursive find-in-files engine.

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "workspace/FileSearch.h"

using hungryeditor::FileSearchHit;
using hungryeditor::FileSearchLimits;
using hungryeditor::FileSearchOptions;
using hungryeditor::searchDirectory;

namespace {

void write(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(bytes);
}

} // namespace

class TestFileSearch : public QObject
{
    Q_OBJECT

private slots:
    void findsMatchesAcrossFilesAndSubdirectories();
    void honoursCaseWholeWordAndRegex();
    void skipsBinaryAndOversizedFiles();
    void honoursTheHitCap();
};

void TestFileSearch::findsMatchesAcrossFilesAndSubdirectories()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("a.md")), "hello world\ngoodbye\n");
    write(dir.filePath(QStringLiteral("nested/b.txt")), "say hello again\n");
    write(dir.filePath(QStringLiteral("c.png")), "hello"); // wrong extension

    const QList<FileSearchHit> hits =
        searchDirectory(dir.path(), QStringLiteral("hello"), FileSearchOptions{});

    QCOMPARE(hits.size(), 2);
    const QStringList names{QFileInfo(hits.at(0).path).fileName(),
                            QFileInfo(hits.at(1).path).fileName()};
    QVERIFY(names.contains(QStringLiteral("a.md")));
    QVERIFY(names.contains(QStringLiteral("b.txt")));
    for (const FileSearchHit& hit : hits) {
        QCOMPARE(hit.line, 0);
        QVERIFY(hit.preview.contains(QStringLiteral("hello")));
    }
}

void TestFileSearch::honoursCaseWholeWordAndRegex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("f.md")), "Cat cat CAT cats\n");

    FileSearchOptions o;
    QCOMPARE(searchDirectory(dir.path(), QStringLiteral("cat"), o).size(), 4);

    o.matchCase = true;
    QCOMPARE(searchDirectory(dir.path(), QStringLiteral("cat"), o).size(), 2); // cat, cats

    o.matchCase = false;
    o.wholeWord = true;
    QCOMPARE(searchDirectory(dir.path(), QStringLiteral("cat"), o).size(), 3); // not "cats"

    FileSearchOptions rx;
    rx.regex = true;
    QCOMPARE(searchDirectory(dir.path(), QStringLiteral("c.t"), rx).size(), 4);
}

void TestFileSearch::skipsBinaryAndOversizedFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("text.md")), "keeps the match\n");
    write(dir.filePath(QStringLiteral("binary.md")), QByteArray("has a match\0here", 15));
    write(dir.filePath(QStringLiteral("huge.md")), QByteArray(200, 'x') + "match\n");

    FileSearchLimits limits;
    limits.maxFileBytes = 64;

    const QList<FileSearchHit> hits =
        searchDirectory(dir.path(), QStringLiteral("match"), FileSearchOptions{}, limits);
    QCOMPARE(hits.size(), 1);
    QCOMPARE(QFileInfo(hits.at(0).path).fileName(), QStringLiteral("text.md"));
}

void TestFileSearch::honoursTheHitCap()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath(QStringLiteral("many.md")), "x x x x x x x x\n");

    FileSearchLimits limits;
    limits.maxHits = 3;
    QCOMPARE(searchDirectory(dir.path(), QStringLiteral("x"), FileSearchOptions{}, limits).size(),
             3);
}

QTEST_APPLESS_MAIN(TestFileSearch)
#include "test_file_search.moc"
