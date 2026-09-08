// Proves the vendored static libraries link and expose usable symbols.
// Runs headless via the Qt "offscreen" platform plugin.

#include <cstdio>
#include <cstring>
#include <string_view>

#include <QApplication>

#include <ILexer.h>
#include <Lexilla.h>
#include <ScintillaEditBase.h>
#include <ScintillaMessages.h>
#include <ScintillaTypes.h>
#include <tree_sitter/api.h>

extern "C" const TSLanguage* tree_sitter_markdown(void);
extern "C" const TSLanguage* tree_sitter_markdown_inline(void);

namespace {

bool contains_type(TSNode node, std::string_view wanted)
{
    if (std::string_view{ts_node_type(node)} == wanted) {
        return true;
    }
    const uint32_t n = ts_node_child_count(node);
    for (uint32_t i = 0; i < n; ++i) {
        if (contains_type(ts_node_child(node, i), wanted)) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    // --- Lexilla: catalogue is populated and known lexers resolve --------
    const int count = GetLexerCount();
    if (count < 100) {
        std::fprintf(stderr, "lexilla: expected 100+ lexers, got %d\n", count);
        return 1;
    }
    for (const char* name : {"cpp", "python", "markdown", "rust", "bash"}) {
        Scintilla::ILexer5* lexer = CreateLexer(name);
        if (!lexer) {
            std::fprintf(stderr, "lexilla: CreateLexer(\"%s\") returned null\n", name);
            return 1;
        }
        lexer->Release();
    }

    // --- Scintilla: construct the widget and round-trip text through it --
    ScintillaEditBase editor;
    editor.send(static_cast<unsigned int>(Scintilla::Message::AddText), 5,
                reinterpret_cast<Scintilla::sptr_t>("hello"));
    const auto length = editor.send(static_cast<unsigned int>(Scintilla::Message::GetLength));
    if (length != 5) {
        std::fprintf(stderr, "scintilla: expected length 5, got %lld\n",
                     static_cast<long long>(length));
        return 1;
    }

    // --- tree-sitter: parse Markdown and inspect the syntax tree ---------
    const char* doc = "# Title\n\nsome text\n\n```rust\nfn main() {}\n```\n";

    TSParser* parser = ts_parser_new();
    if (!ts_parser_set_language(parser, tree_sitter_markdown())) {
        std::fprintf(stderr, "tree-sitter: set_language(markdown) failed (ABI mismatch)\n");
        return 1;
    }
    TSTree* tree =
        ts_parser_parse_string(parser, nullptr, doc, static_cast<uint32_t>(std::strlen(doc)));
    TSNode root = ts_tree_root_node(tree);

    if (std::string_view{ts_node_type(root)} != "document") {
        std::fprintf(stderr, "tree-sitter: root node is \"%s\", expected \"document\"\n",
                     ts_node_type(root));
        return 1;
    }
    if (!contains_type(root, "fenced_code_block")) {
        std::fprintf(stderr, "tree-sitter: no fenced_code_block found in tree\n");
        return 1;
    }

    // The inline grammar must also load (used for injections in Phase 1).
    if (!tree_sitter_markdown_inline()) {
        std::fprintf(stderr, "tree-sitter: markdown_inline language is null\n");
        return 1;
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    std::printf("ok: %d lexers; scintilla buffer length %lld; markdown tree parsed\n", count,
                static_cast<long long>(length));
    return 0;
}
