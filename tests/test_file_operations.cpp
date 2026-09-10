// Coverage for the sidebar's filesystem operations.

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "workspace/FileOperations.h"

namespace fileops = hungryeditor::fileops;

class TestFileOperations : public QObject
{
    Q_OBJECT

private slots:
    void createsAFile();
    void rejectsABadName();
    void refusesToOverwrite();
    void createsAFolder();
    void renamesInPlace();
    void renameRejectsACollision();
    void deleteRemovesAFile();
    void deleteRemovesAPopulatedFolder();
};

void TestFileOperations::createsAFile()
{
    QTemporaryDir dir;
    const fileops::Result r = fileops::createFile(dir.path(), QStringLiteral("notes.md"));
    QVERIFY2(r.ok, qPrintable(r.error));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("notes.md"))));
    QCOMPARE(r.path, QDir(dir.path()).absoluteFilePath(QStringLiteral("notes.md")));
}

void TestFileOperations::rejectsABadName()
{
    QTemporaryDir dir;
    QVERIFY(!fileops::createFile(dir.path(), QStringLiteral("a/b.md")).ok);
    QVERIFY(!fileops::createFile(dir.path(), QString()).ok);
    QVERIFY(!fileops::createFile(dir.path(), QStringLiteral("..")).ok);
}

void TestFileOperations::refusesToOverwrite()
{
    QTemporaryDir dir;
    QVERIFY(fileops::createFile(dir.path(), QStringLiteral("dup.md")).ok);
    const fileops::Result again = fileops::createFile(dir.path(), QStringLiteral("dup.md"));
    QVERIFY(!again.ok);
    QVERIFY(!again.error.isEmpty());
}

void TestFileOperations::createsAFolder()
{
    QTemporaryDir dir;
    const fileops::Result r = fileops::createFolder(dir.path(), QStringLiteral("sub"));
    QVERIFY2(r.ok, qPrintable(r.error));
    QVERIFY(QFileInfo(dir.filePath(QStringLiteral("sub"))).isDir());
    QVERIFY(!fileops::createFolder(dir.path(), QStringLiteral("sub")).ok); // already there
}

void TestFileOperations::renamesInPlace()
{
    QTemporaryDir dir;
    fileops::createFile(dir.path(), QStringLiteral("old.md"));
    const fileops::Result r =
        fileops::rename(dir.filePath(QStringLiteral("old.md")), QStringLiteral("new.md"));
    QVERIFY2(r.ok, qPrintable(r.error));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("old.md"))));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("new.md"))));
    QCOMPARE(r.path, QDir(dir.path()).absoluteFilePath(QStringLiteral("new.md")));
}

void TestFileOperations::renameRejectsACollision()
{
    QTemporaryDir dir;
    fileops::createFile(dir.path(), QStringLiteral("a.md"));
    fileops::createFile(dir.path(), QStringLiteral("b.md"));
    QVERIFY(!fileops::rename(dir.filePath(QStringLiteral("a.md")), QStringLiteral("b.md")).ok);
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("a.md")))); // untouched
}

void TestFileOperations::deleteRemovesAFile()
{
    QTemporaryDir dir;
    fileops::createFile(dir.path(), QStringLiteral("gone.md"));
    QVERIFY(fileops::moveToTrash(dir.filePath(QStringLiteral("gone.md"))).ok);
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("gone.md"))));
}

void TestFileOperations::deleteRemovesAPopulatedFolder()
{
    QTemporaryDir dir;
    fileops::createFolder(dir.path(), QStringLiteral("box"));
    fileops::createFile(dir.filePath(QStringLiteral("box")), QStringLiteral("inside.md"));
    QVERIFY(fileops::moveToTrash(dir.filePath(QStringLiteral("box"))).ok);
    QVERIFY(!QFileInfo(dir.filePath(QStringLiteral("box"))).exists());
}

QTEST_APPLESS_MAIN(TestFileOperations)
#include "test_file_operations.moc"
