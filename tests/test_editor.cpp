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

QTEST_MAIN(TestEditor)
#include "test_editor.moc"
