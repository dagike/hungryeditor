// Coverage for the fenced-code language registry.

#include <array>
#include <string_view>

#include <QtTest>

#include "highlight/GrammarRegistry.h"

namespace {
// A registered name for every vendored grammar.
constexpr std::array<std::string_view, 19> kAllGrammars{
    "bash", "c",      "cpp",  "csharp", "css", "go",   "html",       "java", "javascript", "json",
    "php",  "python", "ruby", "rust",   "sql", "toml", "typescript", "tsx",  "yaml"};
} // namespace

class TestGrammarRegistry : public QObject
{
    Q_OBJECT

private slots:
    void resolvesEveryGrammar();
    void resolvesAliasesCaseInsensitively();
    void unknownNamesReturnEmpty();
};

void TestGrammarRegistry::resolvesEveryGrammar()
{
    for (const std::string_view name : kAllGrammars) {
        const auto grammar = hungryeditor::grammarForName(name);
        QVERIFY2(grammar.language != nullptr, name.data());
        QVERIFY2(grammar.highlights.size() > 10, name.data());
    }
}

void TestGrammarRegistry::resolvesAliasesCaseInsensitively()
{
    const auto rs = hungryeditor::grammarForName("rs");
    const auto rust = hungryeditor::grammarForName("Rust");
    QVERIFY(rs.language != nullptr);
    QCOMPARE(rs.language, rust.language);
    QCOMPARE(rs.highlights.data(), rust.highlights.data());

    QCOMPARE(hungryeditor::grammarForName("PY").language,
             hungryeditor::grammarForName("python").language);
    QCOMPARE(hungryeditor::grammarForName("C++").language,
             hungryeditor::grammarForName("cpp").language);

    const auto sql = hungryeditor::grammarForName("sql");
    QVERIFY(sql.language != nullptr);
    QCOMPARE(hungryeditor::grammarForName("PostgreSQL").language, sql.language);
    QCOMPARE(hungryeditor::grammarForName("psql").language, sql.language);
    QCOMPARE(hungryeditor::grammarForName("MySQL").language, sql.language);
}

void TestGrammarRegistry::unknownNamesReturnEmpty()
{
    QCOMPARE(hungryeditor::grammarForName("nonesuch").language, nullptr);
    QCOMPARE(hungryeditor::grammarForName("").language, nullptr);
    QVERIFY(hungryeditor::grammarForName("brainfuck").highlights.empty());
}

QTEST_MAIN(TestGrammarRegistry)
#include "test_grammar_registry.moc"
