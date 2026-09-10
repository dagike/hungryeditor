// Coverage for the shared colour theme and its generated preview CSS.

#include <QtTest>

#include "theme/Theme.h"

using hungryeditor::Theme;

class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void builtinPaletteLinesUpWithTheEditor();
    void previewCssDefinesTokensAndBaseRules();
};

void TestTheme::builtinPaletteLinesUpWithTheEditor()
{
    const Theme theme = Theme::builtin();
    QVERIFY(theme.background.isValid());
    QVERIFY(theme.text.isValid());
    // Same hexes the editor's markdown styling uses, so the panes match.
    QCOMPARE(theme.heading.name(), QStringLiteral("#0550ae"));
    QCOMPARE(theme.link.name(), QStringLiteral("#0969da"));
    QCOMPARE(theme.codeText.name(), QStringLiteral("#6e40c9"));
}

void TestTheme::previewCssDefinesTokensAndBaseRules()
{
    const QString css = Theme::builtin().previewCss();

    QVERIFY(css.contains(QStringLiteral("--he-bg: #ffffff")));
    QVERIFY(css.contains(QStringLiteral("--he-heading: #0550ae")));
    QVERIFY(css.contains(QStringLiteral("--he-link: #0969da")));

    QVERIFY(
        css.contains(QStringLiteral("body { background: var(--he-bg); color: var(--he-fg); }")));
    QVERIFY(css.contains(QStringLiteral("h1, h2, h3, h4, h5, h6 { color: var(--he-heading)")));
    QVERIFY(css.contains(QStringLiteral("blockquote {")));
    QVERIFY(css.contains(QStringLiteral("pre {")));
    QVERIFY(css.contains(QStringLiteral("th, td { border: 1px solid var(--he-border)")));
    QVERIFY(css.contains(QStringLiteral("li.task-list-item { list-style: none;")));
    QVERIFY(css.contains(QStringLiteral(".footnotes {")));

    // Fenced-code token classes, coloured from the editor's palette.
    QVERIFY(css.contains(QStringLiteral(".tok-keyword { color: #cf222e")));
    QVERIFY(css.contains(QStringLiteral(".tok-comment { color: #6e7781; font-style: italic;")));
    QVERIFY(css.contains(QStringLiteral(".tok-string-escape {")));
}

QTEST_APPLESS_MAIN(TestTheme)
#include "test_theme.moc"
