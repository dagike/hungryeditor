// Coverage for parsing user theme JSON files.

#include <QTemporaryDir>
#include <QtTest>

#include "theme/ThemeFile.h"

using hungryeditor::Theme;
using hungryeditor::themefile::loadThemeFile;
using hungryeditor::themefile::Result;

namespace {

QString writeThemeFile(QTemporaryDir& dir, const QByteArray& content)
{
    const QString path = dir.filePath(QStringLiteral("theme.json"));
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(content);
    file.close();
    return path;
}

} // namespace

class TestThemeFile : public QObject
{
    Q_OBJECT

private slots:
    void appliesOnlyTheGivenOverrides();
    void carriesCustomCssVerbatim();
    void ignoresAnInvalidColourAndFallsBack();
    void reportsAMissingFile();
    void reportsMalformedJson();
};

void TestThemeFile::appliesOnlyTheGivenOverrides()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeThemeFile(dir, R"({"background": "#101010", "heading": "#ff00ff"})");

    const Result result = loadThemeFile(path);
    QVERIFY(result.ok);
    QCOMPARE(result.theme.background.name(), QStringLiteral("#101010"));
    QCOMPARE(result.theme.heading.name(), QStringLiteral("#ff00ff"));
    // Everything else falls back to the light theme.
    const Theme light = Theme::builtin();
    QCOMPARE(result.theme.text, light.text);
    QCOMPARE(result.theme.link, light.link);
    QCOMPARE(result.theme.border, light.border);
}

void TestThemeFile::carriesCustomCssVerbatim()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeThemeFile(dir, R"({"css": "body { letter-spacing: .02em; }"})");

    const Result result = loadThemeFile(path);
    QVERIFY(result.ok);
    QCOMPARE(result.customCss, QStringLiteral("body { letter-spacing: .02em; }"));
}

void TestThemeFile::ignoresAnInvalidColourAndFallsBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeThemeFile(dir, R"({"background": "not-a-colour"})");

    const Result result = loadThemeFile(path);
    QVERIFY(result.ok);
    QCOMPARE(result.theme.background, Theme::builtin().background);
}

void TestThemeFile::reportsAMissingFile()
{
    const Result result = loadThemeFile(QStringLiteral("/nonexistent/theme.json"));
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

void TestThemeFile::reportsMalformedJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeThemeFile(dir, "{ not json");

    const Result result = loadThemeFile(path);
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

QTEST_APPLESS_MAIN(TestThemeFile)
#include "test_theme_file.moc"
