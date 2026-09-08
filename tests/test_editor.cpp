// Coverage for the typed editor wrapper.

#include <QSignalSpy>
#include <QtTest>

#include "editor/Editor.h"

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

QTEST_MAIN(TestEditor)
#include "test_editor.moc"
