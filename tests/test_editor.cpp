// Coverage for the typed editor wrapper.

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QtTest>

#include "editor/Editor.h"
#include "highlight/CaptureStyles.h"

class TestEditor : public QObject
{
    Q_OBJECT

private slots:
    void textRoundTrips();
    void reportsLengthAndLineCount();
    void modifiedFlagLifecycle();
    void textChangedSignalFires();
    void undoRedo();
    void cursorPositionReporting();
    void viewportScrollEmitsASignal();
    void multipleSelectionIsEnabled();
    void selectNextOccurrenceGrowsTheSelection();
    void rectangularSelectionSpansEveryLine();
    void findNextSelectsAndWraps();
    void findHonoursCaseWholeWordAndRegex();
    void replaceAllRewritesEveryMatch();
    void movesDuplicatesAndDeletesLines();
    void joinsLines();
    void togglesHtmlCommentIdempotently();
    void matchesBrackets();
    void newlineCarriesIndentAndContinuesLists();
    void togglesInlineFormattingIdempotently();
    void inlineFormattingWrapsEverySelection();
    void setsAndCyclesHeadingLevels();
    void togglesBlockquoteAndListPrefixes();
    void insertsMarkdownLinks();
    void smartPasteWrapsAUrlSelectionInALink();
    void smartPasteLeavesPlainTextAlone();
    void tabNavigatesTableCellsAndAppendsRows();
    void formatTableAlignsColumns();
    void tabOutsideATableIsNotConsumed();
    void togglesTaskCheckboxAndWritesBack();
    void foldsFrontMatterOnRequest();
    void noFrontMatterLeavesTheFoldMarginHidden();
    void visualDefaultsAreApplied();
    void lineNumberMarginGrowsWithLineCount();
    void changingFontReappliesStyling();
    void setTabWidthChangesScintillaTabWidth();
    void setWordWrapTogglesScintillaWrapMode();
    void headingsAndCodeGetSyntaxStyles();
    void plainParagraphStaysUnstyled();
    void inlineEmphasisInProseGetsStyled();
    void fencedRustBlockGetsLanguageColours();
    void largeDocumentsFallBackFromTreeSitter();
};

void TestEditor::textRoundTrips()
{
    hungryeditor::Editor editor;
    const QString content = QStringLiteral("# Heading\n\nSome text with Ünïcode.\n");
    editor.setText(content);
    QCOMPARE(editor.text(), content);
}

void TestEditor::reportsLengthAndLineCount()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("one\ntwo\nthree\n"));
    QCOMPARE(editor.length(), 14);   // bytes
    QCOMPARE(editor.lineCount(), 4); // trailing newline starts a 4th line
}

void TestEditor::modifiedFlagLifecycle()
{
    hungryeditor::Editor editor;
    QSignalSpy spy(&editor, &hungryeditor::Editor::modifiedChanged);

    editor.setText(QStringLiteral("hello"));
    QVERIFY(editor.isModified());

    editor.markClean();
    QVERIFY(!editor.isModified());

    QVERIFY(!spy.isEmpty());
    QCOMPARE(spy.last().at(0).toBool(), false);
}

void TestEditor::textChangedSignalFires()
{
    hungryeditor::Editor editor;
    QSignalSpy spy(&editor, &hungryeditor::Editor::textChanged);
    editor.setText(QStringLiteral("content"));
    QCOMPARE(spy.count(), 1);
}

void TestEditor::undoRedo()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("first"));
    QVERIFY(editor.canUndo());

    editor.undo();
    QCOMPARE(editor.text(), QString());
    QVERIFY(editor.canRedo());

    editor.redo();
    QCOMPARE(editor.text(), QStringLiteral("first"));
}

void TestEditor::cursorPositionReporting()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("line one\nline two\nline three\n"));

    editor.setCursorPosition(1, 4);
    QCOMPARE(editor.cursorLine(), 1);
    QCOMPARE(editor.cursorColumn(), 4);
}

void TestEditor::viewportScrollEmitsASignal()
{
    hungryeditor::Editor editor;
    editor.resize(400, 120);
    QString many;
    for (int i = 0; i < 200; ++i) {
        many += QStringLiteral("line %1\n").arg(i);
    }
    editor.setText(many);

    QSignalSpy spy(&editor, &hungryeditor::Editor::viewportScrolled);
    editor.setFirstVisibleLine(80);

    QVERIFY(!spy.isEmpty());
    QCOMPARE(editor.firstVisibleLine(), 80);
}

void TestEditor::multipleSelectionIsEnabled()
{
    hungryeditor::Editor editor;
    QVERIFY(editor.call().MultipleSelection());
    QVERIFY(editor.call().AdditionalSelectionTyping());
}

void TestEditor::selectNextOccurrenceGrowsTheSelection()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("foo bar foo baz foo\n"));
    editor.setCursorPosition(0, 1); // inside the first "foo"

    editor.selectNextOccurrence(); // select the word under the caret
    QCOMPARE(editor.selectionCount(), 1);
    QCOMPARE(editor.selectionTexts(), QStringList{QStringLiteral("foo")});

    editor.selectNextOccurrence();
    QCOMPARE(editor.selectionCount(), 2);

    editor.selectNextOccurrence();
    QCOMPARE(editor.selectionCount(), 3);
    for (const QString& selected : editor.selectionTexts()) {
        QCOMPARE(selected, QStringLiteral("foo"));
    }

    editor.selectNextOccurrence(); // every occurrence is already selected
    QCOMPARE(editor.selectionCount(), 3);
}

void TestEditor::rectangularSelectionSpansEveryLine()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("abcdef\nabcdef\nabcdef\nabcdef\n"));

    editor.selectColumn(0, 1, 2, 3); // columns 1..3 on lines 0, 1 and 2

    QCOMPARE(editor.call().SelectionMode(), Scintilla::SelectionMode::Rectangle);
    QCOMPARE(editor.selectionCount(), 3);
    for (const QString& selected : editor.selectionTexts()) {
        QCOMPARE(selected, QStringLiteral("bc"));
    }
}

void TestEditor::findNextSelectsAndWraps()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("one two one three one\n"));
    editor.setCursorPosition(0, 0);
    const hungryeditor::Editor::SearchOptions opts;

    QVERIFY(editor.findNext(QStringLiteral("one"), opts));
    QCOMPARE(editor.cursorColumn(), 3); // caret after the first "one"
    QVERIFY(editor.findNext(QStringLiteral("one"), opts));
    QCOMPARE(editor.cursorColumn(), 11); // after the second
    QVERIFY(editor.findNext(QStringLiteral("one"), opts));
    QVERIFY(editor.findNext(QStringLiteral("one"), opts)); // wraps to the first
    QCOMPARE(editor.cursorColumn(), 3);

    QVERIFY(!editor.findNext(QStringLiteral("absent"), opts));
}

void TestEditor::findHonoursCaseWholeWordAndRegex()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("foo Foo foobar\n"));

    hungryeditor::Editor::SearchOptions o;
    QCOMPARE(editor.markAllMatches(QStringLiteral("foo"), o), 3); // case-insensitive substring

    o.matchCase = true;
    QCOMPARE(editor.markAllMatches(QStringLiteral("foo"), o), 2); // "foo", "foobar"

    o.wholeWord = true;
    QCOMPARE(editor.markAllMatches(QStringLiteral("foo"), o), 1); // just the bare "foo"

    hungryeditor::Editor::SearchOptions rx;
    rx.regex = true;
    QCOMPARE(editor.markAllMatches(QStringLiteral("f.o"), rx), 3);
}

void TestEditor::replaceAllRewritesEveryMatch()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("a-a-a\n"));
    QCOMPARE(editor.replaceAll(QStringLiteral("a"), QStringLiteral("bb"), {}), 3);
    QCOMPARE(editor.text(), QStringLiteral("bb-bb-bb\n"));

    editor.setText(QStringLiteral("2024-01-02\n"));
    hungryeditor::Editor::SearchOptions rx;
    rx.regex = true;
    QCOMPARE(editor.replaceAll(QStringLiteral("(\\d+)-(\\d+)-(\\d+)"),
                               QStringLiteral("\\3/\\2/\\1"), rx),
             1);
    QCOMPARE(editor.text(), QStringLiteral("02/01/2024\n"));
}

void TestEditor::movesDuplicatesAndDeletesLines()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("one\ntwo\nthree\n"));

    editor.setCursorPosition(1, 0); // "two"
    editor.moveLinesDown();
    QCOMPARE(editor.text(), QStringLiteral("one\nthree\ntwo\n"));
    editor.moveLinesUp();
    QCOMPARE(editor.text(), QStringLiteral("one\ntwo\nthree\n"));

    editor.setCursorPosition(0, 1);
    editor.duplicateSelection();
    QCOMPARE(editor.text(), QStringLiteral("one\none\ntwo\nthree\n"));

    editor.setCursorPosition(0, 2);
    editor.deleteLines();
    QCOMPARE(editor.text(), QStringLiteral("one\ntwo\nthree\n"));
}

void TestEditor::joinsLines()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("alpha\nbeta\ngamma\n"));
    editor.setCursorPosition(0, 0);
    editor.joinLines();
    QCOMPARE(editor.text(), QStringLiteral("alpha beta\ngamma\n"));
}

void TestEditor::togglesHtmlCommentIdempotently()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("  hello world\n"));

    editor.setCursorPosition(0, 4);
    editor.toggleLineComment();
    QCOMPARE(editor.text(), QStringLiteral("  <!-- hello world -->\n"));

    editor.setCursorPosition(0, 4);
    editor.toggleLineComment();
    QCOMPARE(editor.text(), QStringLiteral("  hello world\n"));

    // A multi-line selection comments every touched line.
    editor.setText(QStringLiteral("first\nsecond\n"));
    editor.call().SetSelection(editor.call().PositionFromLine(1) + 3, 0);
    editor.toggleLineComment();
    QCOMPARE(editor.text(), QStringLiteral("<!-- first -->\n<!-- second -->\n"));
}

void TestEditor::matchesBrackets()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("foo (bar [baz]) qux\n"));
    QCOMPARE(editor.matchingBrace(4), 14); // ( -> )
    QCOMPARE(editor.matchingBrace(14), 4); // ) -> (
    QCOMPARE(editor.matchingBrace(9), 13); // [ -> ]
    QCOMPARE(editor.matchingBrace(0), -1); // not a bracket

    editor.setText(QStringLiteral("unbalanced (\n"));
    QCOMPARE(editor.matchingBrace(11), -1);
}

void TestEditor::newlineCarriesIndentAndContinuesLists()
{
    hungryeditor::Editor editor;

    const auto pressEnterAtEnd = [&](const QString& start) {
        editor.setText(start);
        editor.call().GotoPos(editor.call().LineEndPosition(0));
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        QCoreApplication::sendEvent(&editor, &press);
    };

    pressEnterAtEnd(QStringLiteral("    indented"));
    QCOMPARE(editor.text(), QStringLiteral("    indented\n    "));

    pressEnterAtEnd(QStringLiteral("- first item"));
    QCOMPARE(editor.text(), QStringLiteral("- first item\n- "));

    pressEnterAtEnd(QStringLiteral("3. third"));
    QCOMPARE(editor.text(), QStringLiteral("3. third\n4. "));

    pressEnterAtEnd(QStringLiteral("- ")); // empty bullet
    QCOMPARE(editor.text(), QString());    // the marker and its newline are removed
}

void TestEditor::togglesInlineFormattingIdempotently()
{
    hungryeditor::Editor editor;

    // Wrap a selection, then strip it again.
    editor.setText(QStringLiteral("make me bold please\n"));
    editor.call().SetSelection(12, 8); // "bold"
    editor.toggleInlineFormat(QStringLiteral("**"));
    QCOMPARE(editor.text(), QStringLiteral("make me **bold** please\n"));
    QCOMPARE(editor.selectedText(), QStringLiteral("bold"));
    editor.toggleInlineFormat(QStringLiteral("**"));
    QCOMPARE(editor.text(), QStringLiteral("make me bold please\n"));

    // A bare caret takes the word under it.
    editor.setText(QStringLiteral("emphasise word here\n"));
    editor.setCursorPosition(0, 12); // inside "word"
    editor.toggleInlineFormat(QStringLiteral("*"));
    QCOMPARE(editor.text(), QStringLiteral("emphasise *word* here\n"));

    // Markers already inside the selection are removed.
    editor.setText(QStringLiteral("a `code` b\n"));
    editor.call().SetSelection(8, 2); // "`code`"
    editor.toggleInlineFormat(QStringLiteral("`"));
    QCOMPARE(editor.text(), QStringLiteral("a code b\n"));
}

void TestEditor::inlineFormattingWrapsEverySelection()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("one two three\n"));

    editor.call().SetSelection(3, 0);  // "one"
    editor.call().AddSelection(13, 8); // "three"
    editor.toggleInlineFormat(QStringLiteral("**"));

    QCOMPARE(editor.text(), QStringLiteral("**one** two **three**\n"));
}

void TestEditor::setsAndCyclesHeadingLevels()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("Title\n"));

    editor.setCursorPosition(0, 0);
    editor.setHeadingLevel(1);
    QCOMPARE(editor.text(), QStringLiteral("# Title\n"));

    editor.setHeadingLevel(3);
    QCOMPARE(editor.text(), QStringLiteral("### Title\n"));

    editor.setHeadingLevel(0);
    QCOMPARE(editor.text(), QStringLiteral("Title\n"));

    editor.cycleHeading();
    QCOMPARE(editor.text(), QStringLiteral("# Title\n"));
    for (int i = 0; i < 5; ++i) {
        editor.cycleHeading();
    }
    QCOMPARE(editor.text(), QStringLiteral("###### Title\n"));
    editor.cycleHeading();
    QCOMPARE(editor.text(), QStringLiteral("Title\n"));
}

void TestEditor::togglesBlockquoteAndListPrefixes()
{
    hungryeditor::Editor editor;

    editor.setText(QStringLiteral("alpha\nbeta\ngamma\n"));
    editor.call().SetSelection(editor.call().PositionFromLine(2) + 2, 0);
    editor.toggleBlockquote();
    QCOMPARE(editor.text(), QStringLiteral("> alpha\n> beta\n> gamma\n"));
    editor.call().SetSelection(editor.call().PositionFromLine(2) + 2, 0);
    editor.toggleBlockquote();
    QCOMPARE(editor.text(), QStringLiteral("alpha\nbeta\ngamma\n"));

    const auto selectAll = [&] {
        editor.call().SetSelection(editor.call().PositionFromLine(2) + 2, 0);
    };

    selectAll();
    editor.toggleBulletList();
    QCOMPARE(editor.text(), QStringLiteral("- alpha\n- beta\n- gamma\n"));
    selectAll();
    editor.toggleBulletList();
    QCOMPARE(editor.text(), QStringLiteral("alpha\nbeta\ngamma\n"));

    selectAll();
    editor.toggleNumberedList();
    QCOMPARE(editor.text(), QStringLiteral("1. alpha\n2. beta\n3. gamma\n"));
    selectAll();
    editor.toggleNumberedList();
    QCOMPARE(editor.text(), QStringLiteral("alpha\nbeta\ngamma\n"));
}

void TestEditor::insertsMarkdownLinks()
{
    hungryeditor::Editor editor;

    editor.setText(QStringLiteral("see the docs\n"));
    editor.call().SetSelection(12, 8); // "docs"
    editor.insertLink();
    QCOMPARE(editor.text(), QStringLiteral("see the [docs](url)\n"));
    QCOMPARE(editor.selectedText(), QStringLiteral("url"));

    editor.setText(QStringLiteral("https://example.com\n"));
    editor.call().SetSelection(19, 0);
    editor.insertLink();
    QCOMPARE(editor.text(), QStringLiteral("[](https://example.com)\n"));
}

void TestEditor::smartPasteWrapsAUrlSelectionInALink()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("see the docs here\n"));
    editor.call().SetSelection(12, 8); // "docs"

    QGuiApplication::clipboard()->setText(QStringLiteral("  https://example.com/x  "));
    QVERIFY(editor.handleSmartPaste());
    QCOMPARE(editor.text(), QStringLiteral("see the [docs](https://example.com/x) here\n"));
}

void TestEditor::smartPasteLeavesPlainTextAlone()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("word\n"));

    editor.call().SetSelection(4, 0); // "word" selected, but the clipboard is prose
    QGuiApplication::clipboard()->setText(QStringLiteral("just some text"));
    QVERIFY(!editor.handleSmartPaste());

    editor.call().SetSelection(4, 4); // a URL but no selection to wrap
    QGuiApplication::clipboard()->setText(QStringLiteral("https://example.com"));
    QVERIFY(!editor.handleSmartPaste());

    QCOMPARE(editor.text(), QStringLiteral("word\n"));
}

void TestEditor::tabNavigatesTableCellsAndAppendsRows()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("| A | B |\n|---|---|\n| 1 | 2 |\n"));
    editor.setCursorPosition(0, 2); // inside "A"

    QVERIFY(editor.navigateTableCell(true));
    QCOMPARE(editor.selectedText(), QStringLiteral("B"));

    QVERIFY(editor.navigateTableCell(true));
    QCOMPARE(editor.selectedText(), QStringLiteral("1"));
    QVERIFY(editor.navigateTableCell(true));
    QCOMPARE(editor.selectedText(), QStringLiteral("2"));

    QVERIFY(editor.navigateTableCell(true)); // past the last cell: new row
    QCOMPARE(editor.selectedText(), QString());
    QCOMPARE(editor.text(),
             QStringLiteral("| A   | B   |\n| --- | --- |\n| 1   | 2   |\n|     |     |\n"));

    QVERIFY(editor.navigateTableCell(false));
    QCOMPARE(editor.selectedText(), QStringLiteral("2")); // Shift+Tab walks back
}

void TestEditor::formatTableAlignsColumns()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("| Name | Age |\n|:--|--:|\n| Bob | 3 |\n| Alexander | 42 |\n"));
    editor.setCursorPosition(2, 4);

    editor.formatTable();

    QCOMPARE(editor.text(), QStringLiteral("| Name      | Age |\n"
                                           "| :-------- | --: |\n"
                                           "| Bob       |   3 |\n"
                                           "| Alexander |  42 |\n"));
    QVERIFY(editor.cursorLine() >= 0 && editor.cursorLine() <= 3);
}

void TestEditor::tabOutsideATableIsNotConsumed()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("just prose here\n"));
    editor.call().GotoPos(editor.call().LineEndPosition(0));
    QVERIFY(!editor.navigateTableCell(true));

    editor.formatTable(); // a no-op that must not disturb the buffer
    QCOMPARE(editor.text(), QStringLiteral("just prose here\n"));
}

void TestEditor::togglesTaskCheckboxAndWritesBack()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("- [ ] one\n- [x] two\n1) [ ] three\nplain line\n"));

    editor.setTaskChecked(0, true);
    QCOMPARE(editor.text(), QStringLiteral("- [x] one\n- [x] two\n1) [ ] three\nplain line\n"));

    editor.setTaskChecked(1, false);
    QCOMPARE(editor.text(), QStringLiteral("- [x] one\n- [ ] two\n1) [ ] three\nplain line\n"));

    editor.setTaskChecked(2, true); // ordered-list task marker
    QCOMPARE(editor.text(), QStringLiteral("- [x] one\n- [ ] two\n1) [x] three\nplain line\n"));

    editor.setTaskChecked(1, false); // already unchecked -> no-op
    editor.setTaskChecked(3, true);  // not a task line -> no-op
    QCOMPARE(editor.text(), QStringLiteral("- [x] one\n- [ ] two\n1) [x] three\nplain line\n"));

    editor.undo(); // the ordered-list toggle was one undo step
    QCOMPARE(editor.text(), QStringLiteral("- [x] one\n- [ ] two\n1) [ ] three\nplain line\n"));
}

void TestEditor::foldsFrontMatterOnRequest()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("---\n"
                                  "title: Hi\n"
                                  "tags: [a, b]\n"
                                  "---\n"
                                  "\n"
                                  "# Body\n"));
    QVERIFY(editor.hasFrontMatter());
    QVERIFY(!editor.isFrontMatterFolded());

    editor.setCursorPosition(5, 0); // outside the block
    editor.setFrontMatterFolded(true);
    QVERIFY(editor.isFrontMatterFolded());
    QVERIFY(editor.call().LineVisible(0));  // the header line stays
    QVERIFY(!editor.call().LineVisible(2)); // an interior line is hidden

    editor.setFrontMatterFolded(false);
    QVERIFY(!editor.isFrontMatterFolded());
    QVERIFY(editor.call().LineVisible(2));
}

void TestEditor::noFrontMatterLeavesTheFoldMarginHidden()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("# Just a heading\n\nText.\n"));
    QVERIFY(!editor.hasFrontMatter());

    editor.setFrontMatterFolded(true); // no-op, must not crash
    QVERIFY(!editor.isFrontMatterFolded());
    QCOMPARE(editor.call().MarginWidthN(2), 0);
}

void TestEditor::visualDefaultsAreApplied()
{
    hungryeditor::Editor editor;
    QCOMPARE(editor.call().TabWidth(), 4);
    QVERIFY(!editor.call().UseTabs());
    QCOMPARE(editor.call().WrapMode(), Scintilla::Wrap::None);
    QCOMPARE(editor.call().CaretWidth(), 2);
    QVERIFY(editor.call().MarginWidthN(0) > 0); // line-number margin visible
}

void TestEditor::lineNumberMarginGrowsWithLineCount()
{
    hungryeditor::Editor editor;
    editor.setText(QStringLiteral("one\ntwo\n"));
    const int narrow = editor.call().MarginWidthN(0);

    QString many;
    for (int i = 0; i < 1500; ++i) {
        many += QStringLiteral("line %1\n").arg(i);
    }
    editor.setText(many);
    QVERIFY(editor.call().MarginWidthN(0) > narrow);
}

void TestEditor::changingFontReappliesStyling()
{
    hungryeditor::Editor editor;
    QFont bigger = editor.editorFont();
    bigger.setPointSize(bigger.pointSize() + 6);
    editor.setEditorFont(bigger);

    QCOMPARE(editor.editorFont().pointSize(), bigger.pointSize());
    QCOMPARE(editor.call().TabWidth(), 4); // still applied after re-styling
}

void TestEditor::setTabWidthChangesScintillaTabWidth()
{
    hungryeditor::Editor editor;
    editor.setTabWidth(8);
    QCOMPARE(editor.tabWidth(), 8);
    QCOMPARE(editor.call().TabWidth(), 8);
}

void TestEditor::setWordWrapTogglesScintillaWrapMode()
{
    hungryeditor::Editor editor;
    QVERIFY(!editor.wordWrap());
    QCOMPARE(editor.call().WrapMode(), Scintilla::Wrap::None);

    editor.setWordWrap(true);
    QVERIFY(editor.wordWrap());
    QCOMPARE(editor.call().WrapMode(), Scintilla::Wrap::Word);
}

void TestEditor::headingsAndCodeGetSyntaxStyles()
{
    hungryeditor::Editor editor;
    QSignalSpy spy(&editor, &hungryeditor::Editor::highlightingApplied);

    const QString doc = QStringLiteral("# Title\n\nplain line\n\n```c\nint x;\n```\n");
    editor.setText(doc);
    QVERIFY(spy.wait(2000));

    const QByteArray bytes = doc.toUtf8();
    const int titleAt = static_cast<int>(bytes.indexOf("Title"));
    const int codeAt = static_cast<int>(bytes.indexOf("int x")); // the C keyword "int"

    QCOMPARE(editor.styleAt(titleAt), static_cast<int>(hungryeditor::StyleHeading));
    // The fenced block is highlighted with the C grammar: "int" is a type.
    QCOMPARE(editor.styleAt(codeAt), static_cast<int>(hungryeditor::StyleType));
}

void TestEditor::plainParagraphStaysUnstyled()
{
    hungryeditor::Editor editor;
    QSignalSpy spy(&editor, &hungryeditor::Editor::highlightingApplied);

    const QString doc = QStringLiteral("# Title\n\njust some prose here\n");
    editor.setText(doc);
    QVERIFY(spy.wait(2000));

    const int proseAt = static_cast<int>(doc.toUtf8().indexOf("prose"));
    QCOMPARE(editor.styleAt(proseAt), static_cast<int>(hungryeditor::StylePlain));
}

void TestEditor::inlineEmphasisInProseGetsStyled()
{
    hungryeditor::Editor editor;
    QSignalSpy spy(&editor, &hungryeditor::Editor::highlightingApplied);

    // Emphasis and strong come from the markdown-inline sub-grammar, run as an
    // injection over the paragraph's inline content.
    const QString doc = QStringLiteral("a paragraph with *soft* and **loud** words\n");
    editor.setText(doc);
    QVERIFY(spy.wait(2000));

    const QByteArray bytes = doc.toUtf8();
    QCOMPARE(editor.styleAt(static_cast<int>(bytes.indexOf("soft"))),
             static_cast<int>(hungryeditor::StyleEmphasis));
    QCOMPARE(editor.styleAt(static_cast<int>(bytes.indexOf("loud"))),
             static_cast<int>(hungryeditor::StyleStrong));
    QCOMPARE(editor.styleAt(static_cast<int>(bytes.indexOf("paragraph"))),
             static_cast<int>(hungryeditor::StylePlain));
}

void TestEditor::fencedRustBlockGetsLanguageColours()
{
    hungryeditor::Editor editor;
    QSignalSpy spy(&editor, &hungryeditor::Editor::highlightingApplied);

    const QString doc = QStringLiteral("# t\n\n```rust\nfn demo() -> i32 { 0 }\n```\n");
    editor.setText(doc);
    QVERIFY(spy.wait(2000));

    const QByteArray bytes = doc.toUtf8();
    QCOMPARE(editor.styleAt(static_cast<int>(bytes.indexOf("fn demo"))),
             static_cast<int>(hungryeditor::StyleKeyword));
    QCOMPARE(editor.styleAt(static_cast<int>(bytes.indexOf("i32"))),
             static_cast<int>(hungryeditor::StyleType));
}

void TestEditor::largeDocumentsFallBackFromTreeSitter()
{
    hungryeditor::Editor editor;
    QSignalSpy spy(&editor, &hungryeditor::Editor::highlightingApplied);
    editor.setFallbackByteLimits(80, 200);

    // Small: tree-sitter drives the colouring.
    editor.setText(QStringLiteral("# Heading\n\nshort body\n"));
    QVERIFY(spy.wait(2000));
    QCOMPARE(editor.highlightTier(), hungryeditor::Editor::HighlightTier::TreeSitter);
    const int headingAt = static_cast<int>(QByteArray("# Heading").indexOf("Heading"));
    QCOMPARE(editor.styleAt(headingAt), static_cast<int>(hungryeditor::StyleHeading));

    // Mid-size: Lexilla's stock lexer takes over.
    editor.setText(QStringLiteral("# Heading\n\n") + QString(120, QLatin1Char('x')) +
                   QStringLiteral("\n"));
    QCOMPARE(editor.highlightTier(), hungryeditor::Editor::HighlightTier::Lexilla);

    // Large: no styling at all.
    editor.setText(QStringLiteral("# Heading\n\n") + QString(400, QLatin1Char('y')) +
                   QStringLiteral("\n"));
    QCOMPARE(editor.highlightTier(), hungryeditor::Editor::HighlightTier::PlainText);
    for (int pos = 0; pos < editor.length(); pos += 37) {
        QCOMPARE(editor.styleAt(pos), static_cast<int>(hungryeditor::StylePlain));
    }

    // Shrinking back restores tree-sitter highlighting.
    QSignalSpy again(&editor, &hungryeditor::Editor::highlightingApplied);
    editor.setText(QStringLiteral("# Heading\n\nshort again\n"));
    QVERIFY(again.wait(2000));
    QCOMPARE(editor.highlightTier(), hungryeditor::Editor::HighlightTier::TreeSitter);
    QCOMPARE(editor.styleAt(headingAt), static_cast<int>(hungryeditor::StyleHeading));
}

QTEST_MAIN(TestEditor)
#include "test_editor.moc"
