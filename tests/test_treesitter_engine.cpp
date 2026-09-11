// Coverage for the incremental tree-sitter parse engine.

#include <cstring>
#include <string>
#include <string_view>

#include <QtTest>

#include "highlight/TreeSitterEngine.h"

extern "C" const TSLanguage* tree_sitter_markdown(void);

namespace {

bool containsType(TSNode node, std::string_view wanted)
{
    if (std::string_view{ts_node_type(node)} == wanted) {
        return true;
    }
    for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
        if (containsType(ts_node_child(node, i), wanted)) {
            return true;
        }
    }
    return false;
}

/// Build the TSInputEdit for replacing [start, oldEnd) in `before` with text
/// that ends at `newEnd` in `after`.
TSInputEdit makeEdit(std::string_view before, std::string_view after, uint32_t start,
                     uint32_t oldEnd, uint32_t newEnd)
{
    return TSInputEdit{
        start,
        oldEnd,
        newEnd,
        hungryeditor::TreeSitterEngine::pointAt(before, start),
        hungryeditor::TreeSitterEngine::pointAt(before, oldEnd),
        hungryeditor::TreeSitterEngine::pointAt(after, newEnd),
    };
}

} // namespace

class TestTreeSitterEngine : public QObject
{
    Q_OBJECT

private slots:
    void fullParseYieldsDocumentRoot();
    void parsingWithoutLanguageIsInert();
    void incrementalInsertReflectsNewContent();
    void incrementalEditCanRemoveAFencedBlock();
    void pointAtComputesRowAndColumn();
    void pointAfterWalksFromAGivenStart();
    void multipleNotedEditsFoldIntoOneReparse();
    void moveTransfersOwnership();
};

void TestTreeSitterEngine::fullParseYieldsDocumentRoot()
{
    hungryeditor::TreeSitterEngine engine;
    engine.setLanguage(tree_sitter_markdown());
    engine.setText("# Title\n\nbody text\n");

    QVERIFY(engine.hasTree());
    QCOMPARE(QByteArray(ts_node_type(engine.rootNode())), QByteArray("document"));
}

void TestTreeSitterEngine::parsingWithoutLanguageIsInert()
{
    hungryeditor::TreeSitterEngine engine;
    engine.setText("# no grammar set\n");
    QVERIFY(!engine.hasTree());
}

void TestTreeSitterEngine::incrementalInsertReflectsNewContent()
{
    hungryeditor::TreeSitterEngine engine;
    engine.setLanguage(tree_sitter_markdown());

    const std::string before = "para one\n";
    engine.setText(before);
    QVERIFY(!containsType(engine.rootNode(), "fenced_code_block"));

    // Append a fenced code block.
    const std::string added = "\n```c\nint x;\n```\n";
    const std::string after = before + added;
    const auto edit =
        makeEdit(before, after, static_cast<uint32_t>(before.size()),
                 static_cast<uint32_t>(before.size()), static_cast<uint32_t>(after.size()));
    engine.applyEdit(edit, after);

    QCOMPARE(engine.source(), std::string_view(after));
    QVERIFY(containsType(engine.rootNode(), "fenced_code_block"));
}

void TestTreeSitterEngine::incrementalEditCanRemoveAFencedBlock()
{
    hungryeditor::TreeSitterEngine engine;
    engine.setLanguage(tree_sitter_markdown());

    const std::string before = "intro\n\n```py\nx = 1\n```\n\noutro\n";
    engine.setText(before);
    QVERIFY(containsType(engine.rootNode(), "fenced_code_block"));

    // Delete the fenced block (from the blank line before it to just before "outro").
    const auto fenceStart = static_cast<uint32_t>(before.find("```"));
    const auto fenceEnd =
        static_cast<uint32_t>(before.rfind("```")) + 4; // include closing fence + \n
    std::string after = before.substr(0, fenceStart) + before.substr(fenceEnd);

    const auto edit = makeEdit(before, after, fenceStart, fenceEnd, fenceStart);
    engine.applyEdit(edit, after);

    QVERIFY(!containsType(engine.rootNode(), "fenced_code_block"));
}

void TestTreeSitterEngine::pointAtComputesRowAndColumn()
{
    const std::string_view text = "abc\ndefg\nhi";
    const TSPoint start = hungryeditor::TreeSitterEngine::pointAt(text, 0);
    QCOMPARE(start.row, 0u);
    QCOMPARE(start.column, 0u);

    const TSPoint mid = hungryeditor::TreeSitterEngine::pointAt(text, 6); // 'e' on line 1
    QCOMPARE(mid.row, 1u);
    QCOMPARE(mid.column, 2u);

    const TSPoint past = hungryeditor::TreeSitterEngine::pointAt(text, 999);
    QCOMPARE(past.row, 2u);
    QCOMPARE(past.column, 2u);
}

void TestTreeSitterEngine::pointAfterWalksFromAGivenStart()
{
    const TSPoint start{2, 3};

    const TSPoint noNewlines = hungryeditor::TreeSitterEngine::pointAfter(start, "abc");
    QCOMPARE(noNewlines.row, 2u);
    QCOMPARE(noNewlines.column, 6u);

    const TSPoint withNewlines = hungryeditor::TreeSitterEngine::pointAfter(start, "ab\ncd\nefg");
    QCOMPARE(withNewlines.row, 4u);
    QCOMPARE(withNewlines.column, 3u);
}

void TestTreeSitterEngine::multipleNotedEditsFoldIntoOneReparse()
{
    // The exact shape HighlightWorker uses for edits that land within one
    // debounce window: several noteEdit() calls against the same tree,
    // then a single reparse() at the end — not one reparse per edit.
    hungryeditor::TreeSitterEngine engine;
    engine.setLanguage(tree_sitter_markdown());

    const std::string text0 = "intro\n";
    engine.setText(text0);
    QVERIFY(!containsType(engine.rootNode(), "fenced_code_block"));

    const std::string text1 = text0 + "\n```c\nint x;\n```\n";
    const auto edit1 =
        makeEdit(text0, text1, static_cast<uint32_t>(text0.size()),
                 static_cast<uint32_t>(text0.size()), static_cast<uint32_t>(text1.size()));

    const std::string text2 = text1 + "\noutro\n";
    const auto edit2 =
        makeEdit(text1, text2, static_cast<uint32_t>(text1.size()),
                 static_cast<uint32_t>(text1.size()), static_cast<uint32_t>(text2.size()));

    engine.noteEdit(edit1);
    engine.noteEdit(edit2);
    engine.reparse(text2);

    QCOMPARE(engine.source(), std::string_view(text2));
    QVERIFY(containsType(engine.rootNode(), "fenced_code_block"));

    // Must match a plain from-scratch parse of the same final text: folding
    // several edits into one reparse has to produce the same tree a full
    // reparse would, just faster.
    hungryeditor::TreeSitterEngine fresh;
    fresh.setLanguage(tree_sitter_markdown());
    fresh.setText(text2);
    QCOMPARE(QByteArray(ts_node_type(engine.rootNode())),
             QByteArray(ts_node_type(fresh.rootNode())));
    QCOMPARE(ts_node_named_child_count(engine.rootNode()),
             ts_node_named_child_count(fresh.rootNode()));
}

void TestTreeSitterEngine::moveTransfersOwnership()
{
    hungryeditor::TreeSitterEngine source;
    source.setLanguage(tree_sitter_markdown());
    source.setText("# moved\n");

    hungryeditor::TreeSitterEngine moved = std::move(source);
    QVERIFY(moved.hasTree());
    QCOMPARE(QByteArray(ts_node_type(moved.rootNode())), QByteArray("document"));
}

QTEST_MAIN(TestTreeSitterEngine)
#include "test_treesitter_engine.moc"
