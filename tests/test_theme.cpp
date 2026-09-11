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
    void eachBuiltinHasADistinctPaletteAndName();
    void builtinKeyRoundTripsThroughFromKey();
    void fromKeyFallsBackOnAnUnknownKey();
    void everyBuiltinDefinesTheSyntaxAndChromeColours();
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
    QVERIFY(css.contains(QStringLiteral("--he-error: #cf222e")));

    QVERIFY(
        css.contains(QStringLiteral("body { background: var(--he-bg); color: var(--he-fg); }")));
    QVERIFY(css.contains(QStringLiteral("h1, h2, h3, h4, h5, h6 { color: var(--he-heading)")));
    QVERIFY(css.contains(QStringLiteral("blockquote {")));
    QVERIFY(css.contains(QStringLiteral("pre {")));
    QVERIFY(css.contains(QStringLiteral("th, td { border: 1px solid var(--he-border)")));
    QVERIFY(css.contains(QStringLiteral("li.task-list-item { list-style: none;")));
    QVERIFY(css.contains(QStringLiteral("li.task-list-item > input { margin: 0 .45em 0 -1.35em; "
                                        "cursor: pointer; }")));
    QVERIFY(css.contains(QStringLiteral(".footnotes {")));
    QVERIFY(css.contains(QStringLiteral(".front-matter-card {")));
    QVERIFY(
        css.contains(QStringLiteral(".mermaid-diagram { margin: 1em 0; text-align: center; }")));
    QVERIFY(
        css.contains(QStringLiteral(".mermaid-diagram svg { max-width: 100%; height: auto; }")));
    QVERIFY(css.contains(
        QStringLiteral(".math-display { display: block; overflow-x: auto; margin: 1em 0; }")));
    QVERIFY(css.contains(QStringLiteral("img[data-img-missing], img[data-img-toobig] {")));
    QVERIFY(css.contains(QStringLiteral(".he-render-error {")));
    QVERIFY(css.contains(QStringLiteral(".he-render-error-inline {")));

    // Fenced-code token classes, coloured from the editor's palette.
    QVERIFY(css.contains(QStringLiteral(".tok-keyword { color: #cf222e")));
    QVERIFY(css.contains(QStringLiteral(".tok-comment { color: #6e7781; font-style: italic;")));
    QVERIFY(css.contains(QStringLiteral(".tok-string-escape {")));
}

void TestTheme::eachBuiltinHasADistinctPaletteAndName()
{
    const Theme light = Theme::forBuiltin(Theme::Builtin::Light);
    const Theme dark = Theme::forBuiltin(Theme::Builtin::Dark);
    const Theme highContrast = Theme::forBuiltin(Theme::Builtin::HighContrast);
    const Theme sepia = Theme::forBuiltin(Theme::Builtin::Sepia);

    QCOMPARE(light.background.name(), QStringLiteral("#ffffff"));
    QVERIFY(dark.background != light.background);
    QVERIFY(highContrast.background != light.background);
    QVERIFY(sepia.background != light.background);
    QVERIFY(dark.background != highContrast.background);
    QVERIFY(dark.background != sepia.background);
    QVERIFY(highContrast.background != sepia.background);

    QCOMPARE(Theme::builtinName(Theme::Builtin::Light), QStringLiteral("Light"));
    QCOMPARE(Theme::builtinName(Theme::Builtin::Dark), QStringLiteral("Dark"));
    QCOMPARE(Theme::builtinName(Theme::Builtin::HighContrast), QStringLiteral("High Contrast"));
    QCOMPARE(Theme::builtinName(Theme::Builtin::Sepia), QStringLiteral("Sepia"));

    QCOMPARE(Theme::builtin().background, light.background);
}

void TestTheme::builtinKeyRoundTripsThroughFromKey()
{
    for (const Theme::Builtin id : {Theme::Builtin::Light, Theme::Builtin::Dark,
                                    Theme::Builtin::HighContrast, Theme::Builtin::Sepia}) {
        const QString key = Theme::builtinKey(id);
        QVERIFY(!key.isEmpty());
        QCOMPARE(Theme::builtinFromKey(key), id);
    }
}

void TestTheme::fromKeyFallsBackOnAnUnknownKey()
{
    QCOMPARE(Theme::builtinFromKey(QStringLiteral("nonsense"), Theme::Builtin::Sepia),
             Theme::Builtin::Sepia);
    QCOMPARE(Theme::builtinFromKey(QString()), Theme::Builtin::Light);
}

void TestTheme::everyBuiltinDefinesTheSyntaxAndChromeColours()
{
    for (const Theme::Builtin id : {Theme::Builtin::Light, Theme::Builtin::Dark,
                                    Theme::Builtin::HighContrast, Theme::Builtin::Sepia}) {
        const Theme theme = Theme::forBuiltin(id);
        for (const QColor& colour :
             {theme.keyword, theme.type, theme.function, theme.string, theme.comment,
              theme.currentLine, theme.selection, theme.findMatch, theme.braceMatch}) {
            QVERIFY(colour.isValid());
        }
    }

    // Light keeps today's exact hex values (zero visual change from before
    // these fields existed).
    const Theme light = Theme::forBuiltin(Theme::Builtin::Light);
    QCOMPARE(light.keyword.name(), QStringLiteral("#cf222e"));
    QCOMPARE(light.type.name(), QStringLiteral("#953800"));
    QCOMPARE(light.function.name(), QStringLiteral("#6639ba"));
    QCOMPARE(light.string.name(), QStringLiteral("#0a3069"));
    QCOMPARE(light.comment.name(), QStringLiteral("#6e7781"));
    QCOMPARE(light.currentLine.name(), QStringLiteral("#f2f6fc"));
    QCOMPARE(light.selection.name(), QStringLiteral("#cfe3ff"));
    QCOMPARE(light.findMatch.name(), QStringLiteral("#f0b429"));
    QCOMPARE(light.braceMatch.name(), QStringLiteral("#bfe3c6"));
}

QTEST_APPLESS_MAIN(TestTheme)
#include "test_theme.moc"
