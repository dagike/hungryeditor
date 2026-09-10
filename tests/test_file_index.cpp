// Coverage for the background file index behind quick-open.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "workspace/FileIndex.h"

using hungryeditor::FileIndex;

class TestFileIndex : public QObject
{
    Q_OBJECT

private slots:
    void indexesFilesAndSkipsIgnoredDirectories();
};

void TestFileIndex::indexesFilesAndSkipsIgnoredDirectories()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const auto touch = [&](const QString& relative) {
        const QString path = dir.filePath(relative);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write("x");
    };
    touch(QStringLiteral("readme.md"));
    touch(QStringLiteral("src/main.cpp"));
    touch(QStringLiteral(".git/config"));
    touch(QStringLiteral("node_modules/pkg/index.js"));
    touch(QStringLiteral(".hidden/secret.txt"));

    FileIndex index;
    QSignalSpy refreshed(&index, &FileIndex::refreshed);
    index.setRoot(dir.path());
    QVERIFY(refreshed.wait(5000));

    QStringList relative;
    for (const QString& path : index.files()) {
        relative << QDir(dir.path()).relativeFilePath(path);
    }
    relative.sort();
    QCOMPARE(relative, (QStringList{QStringLiteral("readme.md"), QStringLiteral("src/main.cpp")}));
    QCOMPARE(index.root(), dir.path());
}

QTEST_GUILESS_MAIN(TestFileIndex)
#include "test_file_index.moc"
