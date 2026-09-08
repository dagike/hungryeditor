// Verifies the vendored libraries link and behave: Lexilla's lexer catalogue,
// a headless Scintilla buffer round-trip, and a tree-sitter Markdown parse.

#include <cstring>
#include <string_view>

#include <QtTest>

#include <ILexer.h>
#include <Lexilla.h>
#include <ScintillaEditBase.h>
#include <ScintillaMessages.h>
#include <tree_sitter/api.h>

extern "C" const TSLanguage* tree_sitter_markdown(void);
extern "C" const TSLanguage* tree_sitter_markdown_inline(void);

namespace {

bool containsType(TSNode node, std::string_view wanted)
{
    if (std::string_view{ts_node_type(node)} == wanted) {
        return true;
    }
    const uint32_t n = ts_node_child_count(node);
    for (uint32_t i = 0; i < n; ++i) {
        if (containsType(ts_node_child(node, i), wanted)) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestVendor : public QObject
{
    Q_OBJECT

private slots:
    void lexillaCatalogueIsPopulated();
    void lexillaResolvesKnownLexers_data();
    void lexillaResolvesKnownLexers();
    void scintillaBufferRoundTrip();
    void treeSitterParsesMarkdown();
    void treeSitterInlineGrammarLoads();
};

void TestVendor::lexillaCatalogueIsPopulated()
{
    QVERIFY2(GetLexerCount() >= 100, "expected the full Lexilla lexer catalogue");
}

void TestVendor::lexillaResolvesKnownLexers_data()
{
    QTest::addColumn<QByteArray>("name");
    for (const char* n : {"cpp", "python", "markdown", "rust", "bash", "json", "yaml"}) {
        QTest::newRow(n) << QByteArray(n);
    }
}

void TestVendor::lexillaResolvesKnownLexers()
{
    QFETCH(QByteArray, name);
    Scintilla::ILexer5* lexer = CreateLexer(name.constData());
    QVERIFY2(lexer != nullptr, name.constData());
    lexer->Release();
}

void TestVendor::scintillaBufferRoundTrip()
{
    ScintillaEditBase editor;
    editor.send(static_cast<unsigned int>(Scintilla::Message::AddText), 5,
                reinterpret_cast<Scintilla::sptr_t>("hello"));
    QCOMPARE(editor.send(static_cast<unsigned int>(Scintilla::Message::GetLength)),
             static_cast<Scintilla::sptr_t>(5));
}

void TestVendor::treeSitterParsesMarkdown()
{
    const char* doc = "# Title\n\nsome text\n\n```rust\nfn main() {}\n```\n";

    TSParser* parser = ts_parser_new();
    QVERIFY2(ts_parser_set_language(parser, tree_sitter_markdown()),
             "tree-sitter ABI mismatch for markdown grammar");

    TSTree* tree =
        ts_parser_parse_string(parser, nullptr, doc, static_cast<uint32_t>(std::strlen(doc)));
    const TSNode root = ts_tree_root_node(tree);

    QCOMPARE(QByteArray(ts_node_type(root)), QByteArray("document"));
    QVERIFY(containsType(root, "fenced_code_block"));

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

void TestVendor::treeSitterInlineGrammarLoads()
{
    QVERIFY(tree_sitter_markdown_inline() != nullptr);
}

QTEST_MAIN(TestVendor)
#include "test_vendor.moc"
