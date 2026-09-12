// Coverage for the tree-sitter query predicate evaluator, isolated from any
// real grammar's highlights.scm so each directive can be tested directly.

#include <string>
#include <string_view>

#include <QtTest>

#include <tree_sitter/api.h>

#include "highlight/GrammarRegistry.h"
#include "highlight/QueryPredicates.h"

namespace {

// One (identifier) @cap match for `source`, taken from a query with a single
// top-level pattern (so pattern_index is always 0). `source` must parse as a
// standalone Python expression statement naming one identifier, e.g. "abc".
bool matchIdentifier(std::string_view queryText, std::string_view source)
{
    const hungryeditor::Grammar grammar = hungryeditor::grammarForName("python");
    Q_ASSERT(grammar.language != nullptr);

    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    TSQuery* query =
        ts_query_new(grammar.language, queryText.data(), static_cast<uint32_t>(queryText.size()),
                     &errorOffset, &errorType);
    Q_ASSERT(query != nullptr);

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, grammar.language);
    TSTree* tree = ts_parser_parse_string(parser, nullptr, source.data(),
                                          static_cast<uint32_t>(source.size()));

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));

    TSQueryMatch match{};
    const bool haveMatch = ts_query_cursor_next_match(cursor, &match);
    Q_ASSERT(haveMatch);

    const bool result = hungryeditor::predicates::matchesPredicates(query, match, source);

    ts_query_cursor_delete(cursor);
    ts_tree_delete(tree);
    ts_parser_delete(parser);
    ts_query_delete(query);
    return result;
}

} // namespace

class TestQueryPredicates : public QObject
{
    Q_OBJECT

private slots:
    void eqPassesAndFails();
    void notEqIsTheOpposite();
    void matchEvaluatesRegex();
    void notMatchIsTheOpposite();
    void anyOfChecksEachAlternative();
    void notAnyOfIsTheOpposite();
    void unrecognisedDirectivesDoNotFilter();
    void multipleDirectivesAreCombinedWithAnd();
    void malformedArgumentCountsDoNotFilter();
    void directiveLanguageExtractsSetInjectionLanguage();
};

void TestQueryPredicates::eqPassesAndFails()
{
    const std::string_view q = "((identifier) @cap (#eq? @cap \"abc\"))";
    QVERIFY(matchIdentifier(q, "abc"));
    QVERIFY(!matchIdentifier(q, "xyz"));
}

void TestQueryPredicates::notEqIsTheOpposite()
{
    const std::string_view q = "((identifier) @cap (#not-eq? @cap \"abc\"))";
    QVERIFY(!matchIdentifier(q, "abc"));
    QVERIFY(matchIdentifier(q, "xyz"));
}

void TestQueryPredicates::matchEvaluatesRegex()
{
    const std::string_view q = "((identifier) @cap (#match? @cap \"^[A-Z][A-Z_]*$\"))";
    QVERIFY(matchIdentifier(q, "MAX_SIZE"));
    QVERIFY(!matchIdentifier(q, "maxSize"));
}

void TestQueryPredicates::notMatchIsTheOpposite()
{
    const std::string_view q = "((identifier) @cap (#not-match? @cap \"^[A-Z][A-Z_]*$\"))";
    QVERIFY(!matchIdentifier(q, "MAX_SIZE"));
    QVERIFY(matchIdentifier(q, "maxSize"));
}

void TestQueryPredicates::anyOfChecksEachAlternative()
{
    const std::string_view q = "((identifier) @cap (#any-of? @cap \"foo\" \"bar\"))";
    QVERIFY(matchIdentifier(q, "foo"));
    QVERIFY(matchIdentifier(q, "bar"));
    QVERIFY(!matchIdentifier(q, "baz"));
}

void TestQueryPredicates::notAnyOfIsTheOpposite()
{
    const std::string_view q = "((identifier) @cap (#not-any-of? @cap \"foo\" \"bar\"))";
    QVERIFY(!matchIdentifier(q, "foo"));
    QVERIFY(matchIdentifier(q, "baz"));
}

void TestQueryPredicates::unrecognisedDirectivesDoNotFilter()
{
    // #is-not? is real tree-sitter query syntax (the local-variable-scope
    // hint a few vendored grammars carry) that this evaluator deliberately
    // does not implement — see QueryPredicates.h. It must not reject the
    // match just because it isn't understood.
    const std::string_view q = "((identifier) @cap (#is-not? local))";
    QVERIFY(matchIdentifier(q, "anything"));
}

void TestQueryPredicates::multipleDirectivesAreCombinedWithAnd()
{
    const std::string_view q =
        "((identifier) @cap (#match? @cap \"^[A-Z]+$\") (#not-eq? @cap \"ABC\"))";
    QVERIFY(matchIdentifier(q, "ABD"));  // matches the regex, and isn't "ABC"
    QVERIFY(!matchIdentifier(q, "ABC")); // matches the regex, but is "ABC"
    QVERIFY(!matchIdentifier(q, "abd")); // doesn't match the regex at all
}

void TestQueryPredicates::malformedArgumentCountsDoNotFilter()
{
    // #eq? normally takes exactly two arguments; one is nonsensical, but it
    // must degrade to "not filtered" rather than being treated as a failure.
    const std::string_view q = "((identifier) @cap (#eq? @cap))";
    QVERIFY(matchIdentifier(q, "anything"));
}

void TestQueryPredicates::directiveLanguageExtractsSetInjectionLanguage()
{
    const hungryeditor::Grammar grammar = hungryeditor::grammarForName("python");
    const std::string_view queryText =
        "((identifier) @injection.content (#set! injection.language \"rust\"))";
    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    TSQuery* query =
        ts_query_new(grammar.language, queryText.data(), static_cast<uint32_t>(queryText.size()),
                     &errorOffset, &errorType);
    QVERIFY(query != nullptr);

    QCOMPARE(hungryeditor::predicates::directiveLanguage(query, 0), std::string("rust"));
    ts_query_delete(query);
}

QTEST_APPLESS_MAIN(TestQueryPredicates)
#include "test_query_predicates.moc"
