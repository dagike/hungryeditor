// Coverage for the typed editor wrapper.

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
    void visualDefaultsAreApplied();
    void lineNumberMarginGrowsWithLineCount();
    void changingFontReappliesStyling();
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
