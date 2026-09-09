// Coverage for the open-document model.

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"

using hungryeditor::Document;
using hungryeditor::DocumentManager;
using hungryeditor::Editor;

namespace {

QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& bytes)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(bytes);
    file.close();
    return path;
}

} // namespace

class TestDocumentManager : public QObject
{
    Q_OBJECT

private slots:
    void startsWithOneUntitledDocument();
    void newDocumentBecomesCurrentAndIsNumbered();
    void openLoadsFileAndDedupes();
    void perDocumentDirtyStateIsIsolated();
    void undoHistorySurvivesDocumentSwitch();
    void closingCurrentActivatesNeighbour();
    void closingNonCurrentKeepsCurrent();
    void closingLastLeavesOneUntitled();
    void modifiedChangedSignalCarriesIndex();
    void saveUpdatesPathAndClearsDirty();
};

void TestDocumentManager::startsWithOneUntitledDocument()
{
    Editor editor;
    DocumentManager manager(&editor);

    QCOMPARE(manager.count(), 1);
    QCOMPARE(manager.currentIndex(), 0);
    QVERIFY(manager.current()->isUntitled());
    QCOMPARE(manager.current()->displayName(), QStringLiteral("Untitled"));
}

void TestDocumentManager::newDocumentBecomesCurrentAndIsNumbered()
{
    Editor editor;
    DocumentManager manager(&editor);

    QSignalSpy added(&manager, &DocumentManager::documentAdded);
    QSignalSpy current(&manager, &DocumentManager::currentChanged);

    Document* second = manager.newDocument();
    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.current(), second);
    QCOMPARE(manager.currentIndex(), 1);
    QCOMPARE(second->displayName(), QStringLiteral("Untitled 1"));
    QCOMPARE(added.count(), 1);
    QCOMPARE(current.count(), 1);
}

void TestDocumentManager::openLoadsFileAndDedupes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFile(dir, QStringLiteral("note.md"), "# Note\n\nbody\n");

    Editor editor;
    DocumentManager manager(&editor);

    Document* opened = manager.openDocument(path);
    QVERIFY(opened != nullptr);
    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.current(), opened);
    QCOMPARE(opened->path(), path);
    QVERIFY(!opened->isModified());
    QCOMPARE(editor.text(), QStringLiteral("# Note\n\nbody\n"));

    manager.setCurrentIndex(0);
    Document* again = manager.openDocument(path);
    QCOMPARE(again, opened);
    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.currentIndex(), 1);
}

void TestDocumentManager::perDocumentDirtyStateIsIsolated()
{
    Editor editor;
    DocumentManager manager(&editor);

    editor.setText(QStringLiteral("edited first doc"));
    QVERIFY(manager.current()->isModified());

    Document* first = manager.documentAt(0);
    manager.newDocument();
    QVERIFY(!manager.current()->isModified());
    QVERIFY(first->isModified());

    manager.setCurrentIndex(0);
    QVERIFY(editor.isModified());
    QVERIFY(manager.current()->isModified());
}

void TestDocumentManager::undoHistorySurvivesDocumentSwitch()
{
    Editor editor;
    DocumentManager manager(&editor);

    editor.setText(QStringLiteral("first buffer"));
    manager.newDocument();
    editor.setText(QStringLiteral("second buffer"));

    manager.setCurrentIndex(0);
    QCOMPARE(editor.text(), QStringLiteral("first buffer"));
    QVERIFY(editor.canUndo());
    editor.undo();
    QCOMPARE(editor.text(), QString());

    manager.setCurrentIndex(1);
    QCOMPARE(editor.text(), QStringLiteral("second buffer"));
}

void TestDocumentManager::closingCurrentActivatesNeighbour()
{
    Editor editor;
    DocumentManager manager(&editor);
    manager.newDocument();
    manager.newDocument();
    QCOMPARE(manager.count(), 3);

    manager.setCurrentIndex(1);
    Document* third = manager.documentAt(2);
    manager.closeDocument(1);

    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.current(), third);
    QCOMPARE(manager.currentIndex(), 1);
}

void TestDocumentManager::closingNonCurrentKeepsCurrent()
{
    Editor editor;
    DocumentManager manager(&editor);
    manager.newDocument();
    manager.newDocument(); // current is index 2

    Document* stayCurrent = manager.documentAt(2);
    manager.closeDocument(0);

    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.current(), stayCurrent);
    QCOMPARE(manager.currentIndex(), 1);
}

void TestDocumentManager::closingLastLeavesOneUntitled()
{
    Editor editor;
    DocumentManager manager(&editor);
    editor.setText(QStringLiteral("scratch"));

    manager.closeDocument(0);

    QCOMPARE(manager.count(), 1);
    QVERIFY(manager.current()->isUntitled());
    QCOMPARE(editor.text(), QString());
    QVERIFY(!manager.current()->isModified());
}

void TestDocumentManager::modifiedChangedSignalCarriesIndex()
{
    Editor editor;
    DocumentManager manager(&editor);
    manager.newDocument();

    QSignalSpy spy(&manager, &DocumentManager::modifiedChanged);
    editor.setText(QStringLiteral("dirty now"));

    QVERIFY(!spy.isEmpty());
    const QList<QVariant> last = spy.last();
    QCOMPARE(last.at(0).toInt(), 1);
    QCOMPARE(last.at(1).toBool(), true);
}

void TestDocumentManager::saveUpdatesPathAndClearsDirty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Editor editor;
    DocumentManager manager(&editor);
    editor.setText(QStringLiteral("hello\nworld\n"));
    QVERIFY(manager.current()->isModified());

    const QString path = dir.filePath(QStringLiteral("saved.md"));
    QVERIFY(manager.saveDocument(manager.current(), path));

    QCOMPARE(manager.current()->path(), path);
    QVERIFY(!manager.current()->isModified());
    QCOMPARE(manager.current()->displayName(), QStringLiteral("saved.md"));

    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly));
    QCOMPARE(written.readAll(), QByteArray("hello\nworld\n"));
}

QTEST_MAIN(TestDocumentManager)
#include "test_document_manager.moc"
