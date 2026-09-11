// Coverage for the semantic style table and its theme-driven colour remap.

#include <QtTest>

#include "highlight/CaptureStyles.h"
#include "theme/Theme.h"

using hungryeditor::Style;
using hungryeditor::StyleDef;
using hungryeditor::Theme;
using hungryeditor::themedStyleTable;

namespace {

const StyleDef& find(const std::vector<StyleDef>& table, int id)
{
    for (const StyleDef& def : table) {
        if (def.id == id) {
            return def;
        }
    }
    Q_UNREACHABLE();
}

} // namespace

class TestCaptureStyles : public QObject
{
    Q_OBJECT

private slots:
    void lightThemeMatchesTheStructuralDefaults();
    void aNonLightThemeRemapsColoursButKeepsKeysAndFlags();
    void keywordAndOperatorShareOneThemeField();
};

void TestCaptureStyles::lightThemeMatchesTheStructuralDefaults()
{
    const std::vector<StyleDef> themed = themedStyleTable(Theme::builtin());
    const std::vector<StyleDef>& defaults = hungryeditor::styleTable();

    QCOMPARE(themed.size(), defaults.size());
    for (std::size_t i = 0; i < themed.size(); ++i) {
        QCOMPARE(themed[i].foreground, defaults[i].foreground);
    }
}

void TestCaptureStyles::aNonLightThemeRemapsColoursButKeepsKeysAndFlags()
{
    const Theme dark = Theme::forBuiltin(Theme::Builtin::Dark);
    const std::vector<StyleDef> themed = themedStyleTable(dark);
    const std::vector<StyleDef>& defaults = hungryeditor::styleTable();

    QCOMPARE(find(themed, Style::StyleKeyword).foreground, dark.keyword);
    QCOMPARE(find(themed, Style::StyleType).foreground, dark.type);
    QCOMPARE(find(themed, Style::StyleFunction).foreground, dark.function);
    QCOMPARE(find(themed, Style::StyleString).foreground, dark.string);
    QCOMPARE(find(themed, Style::StyleComment).foreground, dark.comment);
    QCOMPARE(find(themed, Style::StyleHeading).foreground, dark.heading);
    QCOMPARE(find(themed, Style::StyleLink).foreground, dark.link);
    QCOMPARE(find(themed, Style::StyleCodeLiteral).foreground, dark.codeText);
    QCOMPARE(find(themed, Style::StylePunctuation).foreground, dark.muted);
    QCOMPARE(find(themed, Style::StylePlain).foreground, dark.text);

    // Key names and font flags never change with the theme.
    for (std::size_t i = 0; i < themed.size(); ++i) {
        QCOMPARE(themed[i].id, defaults[i].id);
        QCOMPARE(QString::fromLatin1(themed[i].key), QString::fromLatin1(defaults[i].key));
        QCOMPARE(themed[i].bold, defaults[i].bold);
        QCOMPARE(themed[i].italic, defaults[i].italic);
        QCOMPARE(themed[i].underline, defaults[i].underline);
    }
}

void TestCaptureStyles::keywordAndOperatorShareOneThemeField()
{
    const Theme sepia = Theme::forBuiltin(Theme::Builtin::Sepia);
    const std::vector<StyleDef> themed = themedStyleTable(sepia);
    QCOMPARE(find(themed, Style::StyleKeyword).foreground,
             find(themed, Style::StyleOperator).foreground);
    QCOMPARE(find(themed, Style::StyleOperator).foreground, sepia.keyword);
}

QTEST_APPLESS_MAIN(TestCaptureStyles)
#include "test_capture_styles.moc"
