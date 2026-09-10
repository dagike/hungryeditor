// Coverage for pasted-image asset writing.

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

#include "io/AssetWriter.h"

class TestAssetWriter : public QObject
{
    Q_OBJECT

private slots:
    void writesBesideTheDocumentAsARelativePath();
    void fallsBackToTheGivenDirectoryWhenUnsaved();
    void rejectsANullImage();
};

void TestAssetWriter::writesBesideTheDocumentAsARelativePath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString document = dir.filePath(QStringLiteral("notes/post.md"));
    QVERIFY(QDir().mkpath(QFileInfo(document).absolutePath()));

    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::red);

    const QString ref = hungryeditor::assets::writePastedImage(image, document, QString());
    QVERIFY(!ref.isEmpty());
    QVERIFY(!QDir::isAbsolutePath(ref));
    QVERIFY(ref.startsWith(QLatin1String("assets/")));
    QVERIFY(ref.endsWith(QLatin1String(".png")));

    const QString absolute = QDir(QFileInfo(document).absolutePath()).filePath(ref);
    QVERIFY(QFileInfo::exists(absolute));
    QVERIFY(!QImage(absolute).isNull());
}

void TestAssetWriter::fallsBackToTheGivenDirectoryWhenUnsaved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::blue);

    const QString ref = hungryeditor::assets::writePastedImage(image, QString(), dir.path());
    QVERIFY(QDir::isAbsolutePath(ref));
    QVERIFY(ref.contains(QLatin1String("/assets/")));
    QVERIFY(QFileInfo::exists(ref));
}

void TestAssetWriter::rejectsANullImage()
{
    QTemporaryDir dir;
    QCOMPARE(hungryeditor::assets::writePastedImage(QImage(), QString(), dir.path()), QString());
}

QTEST_MAIN(TestAssetWriter)
#include "test_asset_writer.moc"
