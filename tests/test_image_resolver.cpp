// Coverage for inlining local <img> sources into the preview HTML.

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "markdown/ImageResolver.h"

using hungryeditor::images::inlineLocalImages;

namespace {

// A 1x1 transparent PNG.
const char* const kPngBase64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4"
                               "2mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";

} // namespace

class TestImageResolver : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void inlinesARelativeImageAsADataUri();
    void resolvesAnAbsolutePath();
    void aMissingFileLosesItsSrc();
    void anOversizedFileIsDropped();
    void leavesRemoteSourcesUntouched();
    void decodesPercentEscapesInThePath();

private:
    QTemporaryDir dir_;
    QString write(const QString& name, const QByteArray& bytes);
    QString writePng(const QString& name)
    {
        return write(name, QByteArray::fromBase64(kPngBase64));
    }
};

void TestImageResolver::init()
{
    QVERIFY(dir_.isValid());
}

QString TestImageResolver::write(const QString& name, const QByteArray& bytes)
{
    const QString path = dir_.filePath(name);
    QFile file(path);
    const bool ok = file.open(QIODevice::WriteOnly);
    Q_ASSERT(ok);
    file.write(bytes);
    return path;
}

void TestImageResolver::inlinesARelativeImageAsADataUri()
{
    writePng(QStringLiteral("pic.png"));
    const QString out =
        inlineLocalImages(QStringLiteral("<img src=\"pic.png\" alt=\"p\">"), dir_.path());

    QVERIFY(out.contains(QStringLiteral("<img src=\"data:image/png;base64,")));
    QVERIFY(out.contains(QStringLiteral("alt=\"p\">")));
    QVERIFY(!out.contains(QStringLiteral("src=\"pic.png\"")));
}

void TestImageResolver::resolvesAnAbsolutePath()
{
    const QString abs = writePng(QStringLiteral("abs.png"));
    const QString out =
        inlineLocalImages(QStringLiteral("<img src=\"%1\" alt=\"\">").arg(abs), QString());

    QVERIFY(out.contains(QStringLiteral("src=\"data:image/png;base64,")));
}

void TestImageResolver::aMissingFileLosesItsSrc()
{
    const QString out =
        inlineLocalImages(QStringLiteral("<img src=\"nope.png\" alt=\"x\">"), dir_.path());

    QVERIFY(out.contains(QStringLiteral("<img data-img-missing=\"nope.png\" alt=\"x\">")));
    QVERIFY(!out.contains(QStringLiteral("src=")));
}

void TestImageResolver::anOversizedFileIsDropped()
{
    write(QStringLiteral("big.png"), QByteArray(2048, 'x'));
    const QString out =
        inlineLocalImages(QStringLiteral("<img src=\"big.png\" alt=\"\">"), dir_.path(), 512);

    QVERIFY(out.contains(QStringLiteral("data-img-toobig=\"big.png\"")));
    QVERIFY(!out.contains(QStringLiteral("src=")));
}

void TestImageResolver::leavesRemoteSourcesUntouched()
{
    const QString html = QStringLiteral("<img src=\"https://example.com/x.png\" alt=\"\">"
                                        "<img src=\"data:image/gif;base64,AAAA\" alt=\"\">");
    QCOMPARE(inlineLocalImages(html, dir_.path()), html);
}

void TestImageResolver::decodesPercentEscapesInThePath()
{
    writePng(QStringLiteral("my image.png"));
    const QString out =
        inlineLocalImages(QStringLiteral("<img src=\"my%20image.png\" alt=\"\">"), dir_.path());

    QVERIFY(out.contains(QStringLiteral("src=\"data:image/png;base64,")));
}

QTEST_APPLESS_MAIN(TestImageResolver)
#include "test_image_resolver.moc"
