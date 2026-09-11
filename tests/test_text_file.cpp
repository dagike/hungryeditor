// Coverage for encoding and line-ending detection and round-tripping.

#include <string_view>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "io/TextFile.h"

using hungryeditor::Encoding;
using hungryeditor::FileError;
using hungryeditor::LineEnding;
using hungryeditor::TextDocument;

class TestTextFile : public QObject
{
    Q_OBJECT

private slots:
    void decodesPlainUtf8();
    void stripsUtf8Bom();
    void decodesUtf16LeBom();
    void decodesUtf16BeBom();
    void invalidUtf8FallsBackToLatin1();
    void detectsLineEnding_data();
    void detectsLineEnding();
    void normalisesEveryNewlineToLf();
    void roundTrips_data();
    void roundTrips();
    void loadMissingFileReportsError();
    void savesAndReloadsFromDisk();
    void saveIsAtomicOnExistingFile();
    void lineEndingLabelsAreShortAndDistinct();
    void encodingLabelsAreShortAndDistinct();
};

void TestTextFile::decodesPlainUtf8()
{
    const TextDocument doc = hungryeditor::decodeBytes(QByteArray("# Héllo\nwörld\n"));
    QCOMPARE(doc.encoding, Encoding::Utf8);
    QCOMPARE(doc.lineEnding, LineEnding::Lf);
    QCOMPARE(doc.text, QStringLiteral("# Héllo\nwörld\n"));
}

void TestTextFile::stripsUtf8Bom()
{
    QByteArray bytes("\xEF\xBB\xBF", 3);
    bytes += "title\r\nbody\r\n";
    const TextDocument doc = hungryeditor::decodeBytes(bytes);
    QCOMPARE(doc.encoding, Encoding::Utf8Bom);
    QCOMPARE(doc.lineEnding, LineEnding::CrLf);
    QCOMPARE(doc.text, QStringLiteral("title\nbody\n"));
}

void TestTextFile::decodesUtf16LeBom()
{
    QByteArray bytes("\xFF\xFE", 2);
    for (const char16_t ch : std::u16string_view(u"hi\nthere")) {
        bytes += static_cast<char>(ch & 0xFF);
        bytes += static_cast<char>((ch >> 8) & 0xFF);
    }
    const TextDocument doc = hungryeditor::decodeBytes(bytes);
    QCOMPARE(doc.encoding, Encoding::Utf16Le);
    QCOMPARE(doc.text, QStringLiteral("hi\nthere"));
}

void TestTextFile::decodesUtf16BeBom()
{
    QByteArray bytes("\xFE\xFF", 2);
    for (const char16_t ch : std::u16string_view(u"abc")) {
        bytes += static_cast<char>((ch >> 8) & 0xFF);
        bytes += static_cast<char>(ch & 0xFF);
    }
    const TextDocument doc = hungryeditor::decodeBytes(bytes);
    QCOMPARE(doc.encoding, Encoding::Utf16Be);
    QCOMPARE(doc.text, QStringLiteral("abc"));
}

void TestTextFile::invalidUtf8FallsBackToLatin1()
{
    // 0xE9 is "é" in Latin-1 and an invalid lone lead byte in UTF-8.
    const TextDocument doc = hungryeditor::decodeBytes(QByteArray("caf\xE9 open\n"));
    QCOMPARE(doc.encoding, Encoding::Latin1);
    QCOMPARE(doc.text, QStringLiteral("café open\n"));
}

void TestTextFile::detectsLineEnding_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<int>("expected");

    QTest::newRow("lf") << QByteArray("a\nb\nc\n") << int(LineEnding::Lf);
    QTest::newRow("crlf") << QByteArray("a\r\nb\r\nc\r\n") << int(LineEnding::CrLf);
    QTest::newRow("cr") << QByteArray("a\rb\rc\r") << int(LineEnding::Cr);
    QTest::newRow("mixed-mostly-lf") << QByteArray("a\nb\nc\r\nd\n") << int(LineEnding::Lf);
    QTest::newRow("none") << QByteArray("single line") << int(LineEnding::Lf);
}

void TestTextFile::detectsLineEnding()
{
    QFETCH(QByteArray, bytes);
    QFETCH(int, expected);
    QCOMPARE(int(hungryeditor::decodeBytes(bytes).lineEnding), expected);
}

void TestTextFile::normalisesEveryNewlineToLf()
{
    const TextDocument doc = hungryeditor::decodeBytes(QByteArray("a\r\nb\rc\nd"));
    QCOMPARE(doc.text, QStringLiteral("a\nb\nc\nd"));
}

void TestTextFile::roundTrips_data()
{
    QTest::addColumn<QByteArray>("bytes");

    QByteArray utf8Bom("\xEF\xBB\xBF", 3);
    utf8Bom += "with bom\nsecond\n";

    QTest::newRow("utf8-lf") << QByteArray("plain \xC3\xA9 text\nline two\n");
    QTest::newRow("utf8-crlf") << QByteArray("windows\r\nstyle\r\n");
    QTest::newRow("utf8-cr") << QByteArray("mac\rstyle\r");
    QTest::newRow("utf8-bom") << utf8Bom;
    QTest::newRow("latin1") << QByteArray("caf\xE9\nna\xEFve\n");
    QTest::newRow("no-trailing-newline") << QByteArray("just one line");
}

void TestTextFile::roundTrips()
{
    QFETCH(QByteArray, bytes);
    const TextDocument doc = hungryeditor::decodeBytes(bytes);
    QCOMPARE(hungryeditor::encodeDocument(doc), bytes);
}

void TestTextFile::loadMissingFileReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FileError error;
    const TextDocument doc =
        hungryeditor::loadFile(dir.filePath(QStringLiteral("nope.md")), &error);
    QVERIFY(!error.ok);
    QVERIFY(!error.message.isEmpty());
    QVERIFY(doc.text.isEmpty());
}

void TestTextFile::savesAndReloadsFromDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("note.md"));

    TextDocument doc;
    doc.text = QStringLiteral("# Title\n\nbody text\n");
    doc.encoding = Encoding::Utf8;
    doc.lineEnding = LineEnding::CrLf;

    FileError error;
    QVERIFY(hungryeditor::saveFile(path, doc, &error));
    QVERIFY(error.ok);

    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly));
    QCOMPARE(written.readAll(), QByteArray("# Title\r\n\r\nbody text\r\n"));
    written.close();

    const TextDocument reloaded = hungryeditor::loadFile(path, &error);
    QVERIFY(error.ok);
    QCOMPARE(reloaded.text, doc.text);
    QCOMPARE(reloaded.lineEnding, LineEnding::CrLf);
    QCOMPARE(reloaded.encoding, Encoding::Utf8);
}

void TestTextFile::saveIsAtomicOnExistingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("existing.md"));

    TextDocument first;
    first.text = QStringLiteral("original\n");
    QVERIFY(hungryeditor::saveFile(path, first));

    TextDocument second;
    second.text = QStringLiteral("replacement\n");
    QVERIFY(hungryeditor::saveFile(path, second));

    QCOMPARE(hungryeditor::loadFile(path).text, QStringLiteral("replacement\n"));
}

void TestTextFile::lineEndingLabelsAreShortAndDistinct()
{
    QCOMPARE(hungryeditor::lineEndingLabel(LineEnding::Lf), QStringLiteral("LF"));
    QCOMPARE(hungryeditor::lineEndingLabel(LineEnding::CrLf), QStringLiteral("CRLF"));
    QCOMPARE(hungryeditor::lineEndingLabel(LineEnding::Cr), QStringLiteral("CR"));
}

void TestTextFile::encodingLabelsAreShortAndDistinct()
{
    QCOMPARE(hungryeditor::encodingLabel(Encoding::Utf8), QStringLiteral("UTF-8"));
    QCOMPARE(hungryeditor::encodingLabel(Encoding::Utf8Bom), QStringLiteral("UTF-8 BOM"));
    QCOMPARE(hungryeditor::encodingLabel(Encoding::Utf16Le), QStringLiteral("UTF-16 LE"));
    QCOMPARE(hungryeditor::encodingLabel(Encoding::Utf16Be), QStringLiteral("UTF-16 BE"));
    QCOMPARE(hungryeditor::encodingLabel(Encoding::Latin1), QStringLiteral("ANSI"));
}

QTEST_MAIN(TestTextFile)
#include "test_text_file.moc"
