// Coverage for the open-document model.

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "editor/Document.h"
#include "editor/DocumentManager.h"
#include "editor/Editor.h"
#include "io/DraftStore.h"

using hungryeditor::Document;
using hungryeditor::DocumentManager;
using hungryeditor::Draft;
using hungryeditor::DraftStore;
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
    void externalEditIsDetectedAndReloads();
    void savingDoesNotLookLikeAnExternalChange();
    void deletingThenRecreatingTheFileIsReported();
    void watcherDeliversAnExternalEditThroughTheEventLoop();
    void reloadRejectsAnUntitledDocument();
    void autosaveWritesADraftPerDirtyBuffer();
    void autosaveReadsNonCurrentBuffersFromTheirSnapshot();
    void savingAndClosingDropTheDraft();
    void restoreDraftsRecreatesModifiedBuffers();
    void autosaveTimerWritesADraftOnItsOwn();
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

void TestDocumentManager::externalEditIsDetectedAndReloads()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFile(dir, QStringLiteral("watched.md"), "first\n");

    Editor editor;
    DocumentManager manager(&editor);
    Document* opened = manager.openDocument(path);
    QVERIFY(opened != nullptr);

    writeFile(dir, QStringLiteral("watched.md"), "second, longer line\n");

    QSignalSpy changed(&manager, &DocumentManager::fileChangedExternally);
    manager.pollExternalChanges();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().at(0).toInt(), manager.indexOf(opened));

    QVERIFY(manager.reloadDocument(opened));
    QCOMPARE(editor.text(), QStringLiteral("second, longer line\n"));
    QVERIFY(!opened->isModified());

    // A second poll with nothing else changed stays quiet.
    QSignalSpy again(&manager, &DocumentManager::fileChangedExternally);
    manager.pollExternalChanges();
    QCOMPARE(again.count(), 0);
}

void TestDocumentManager::savingDoesNotLookLikeAnExternalChange()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Editor editor;
    DocumentManager manager(&editor);
    editor.setText(QStringLiteral("body\n"));
    const QString path = dir.filePath(QStringLiteral("out.md"));
    QVERIFY(manager.saveDocument(manager.current(), path));

    QSignalSpy changed(&manager, &DocumentManager::fileChangedExternally);
    manager.pollExternalChanges();
    QCOMPARE(changed.count(), 0);
}

void TestDocumentManager::deletingThenRecreatingTheFileIsReported()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFile(dir, QStringLiteral("gone.md"), "here\n");

    Editor editor;
    DocumentManager manager(&editor);
    Document* opened = manager.openDocument(path);
    QVERIFY(opened != nullptr);

    QVERIFY(QFile::remove(path));
    QSignalSpy removed(&manager, &DocumentManager::fileRemovedExternally);
    manager.pollExternalChanges();
    QCOMPARE(removed.count(), 1);
    QCOMPARE(removed.first().at(0).toInt(), manager.indexOf(opened));

    writeFile(dir, QStringLiteral("gone.md"), "back again\n");
    QSignalSpy changed(&manager, &DocumentManager::fileChangedExternally);
    manager.pollExternalChanges();
    QCOMPARE(changed.count(), 1);
}

void TestDocumentManager::watcherDeliversAnExternalEditThroughTheEventLoop()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFile(dir, QStringLiteral("poll.md"), "one\n");

    Editor editor;
    DocumentManager manager(&editor);
    QVERIFY(manager.openDocument(path) != nullptr);

    QSignalSpy changed(&manager, &DocumentManager::fileChangedExternally);
    writeFile(dir, QStringLiteral("poll.md"), "two, changed on disk\n");

    // No manual poll: the QFileSystemWatcher and the debounce timer must run it.
    QVERIFY(changed.wait(5000));
    QCOMPARE(changed.count(), 1);
}

void TestDocumentManager::reloadRejectsAnUntitledDocument()
{
    Editor editor;
    DocumentManager manager(&editor);

    hungryeditor::FileError error;
    QVERIFY(!manager.reloadDocument(manager.current(), &error));
    QVERIFY(!error.ok);
}

void TestDocumentManager::autosaveWritesADraftPerDirtyBuffer()
{
    QTemporaryDir drafts;
    QVERIFY(drafts.isValid());

    Editor editor;
    DocumentManager manager(&editor);
    manager.setDraftDirectory(drafts.path());

    editor.setText(QStringLiteral("first dirty\n"));
    manager.newDocument();
    editor.setText(QStringLiteral("second dirty\n"));

    manager.autosaveDirtyDocuments();

    QStringList texts;
    for (const Draft& draft : DraftStore(drafts.path()).loadAll()) {
        texts << draft.text;
    }
    QCOMPARE(texts.size(), 2);
    QVERIFY(texts.contains(QStringLiteral("first dirty\n")));
    QVERIFY(texts.contains(QStringLiteral("second dirty\n")));
}

void TestDocumentManager::autosaveReadsNonCurrentBuffersFromTheirSnapshot()
{
    QTemporaryDir drafts;
    QVERIFY(drafts.isValid());

    Editor editor;
    DocumentManager manager(&editor);
    manager.setDraftDirectory(drafts.path());

    Document* first = manager.current();
    editor.setText(QStringLiteral("typed into the first buffer\n"));
    manager.newDocument(); // first is no longer current; its text is snapshotted

    manager.autosaveDirtyDocuments();

    bool found = false;
    for (const Draft& draft : DraftStore(drafts.path()).loadAll()) {
        if (draft.id == first->draftId()) {
            QCOMPARE(draft.text, QStringLiteral("typed into the first buffer\n"));
            found = true;
        }
    }
    QVERIFY(found);
}

void TestDocumentManager::savingAndClosingDropTheDraft()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QTemporaryDir drafts;
    QVERIFY(drafts.isValid());

    Editor editor;
    DocumentManager manager(&editor);
    manager.setDraftDirectory(drafts.path());

    editor.setText(QStringLiteral("to be saved\n"));
    manager.newDocument();
    editor.setText(QStringLiteral("to be closed\n"));
    manager.autosaveDirtyDocuments();
    QCOMPARE(DraftStore(drafts.path()).loadAll().size(), 2);

    QVERIFY(manager.saveDocument(manager.documentAt(0), dir.filePath(QStringLiteral("s.md"))));
    QCOMPARE(DraftStore(drafts.path()).loadAll().size(), 1);

    manager.closeDocument(1);
    QCOMPARE(DraftStore(drafts.path()).loadAll().size(), 0);
}

void TestDocumentManager::restoreDraftsRecreatesModifiedBuffers()
{
    QTemporaryDir drafts;
    QVERIFY(drafts.isValid());
    {
        DraftStore seed(drafts.path());
        Draft draft;
        draft.id = QStringLiteral("recover1");
        draft.text = QStringLiteral("work in progress\n");
        QVERIFY(seed.write(draft));
    }

    Editor editor;
    DocumentManager manager(&editor);
    manager.setDraftDirectory(drafts.path());

    const QList<Draft> pending = manager.pendingDrafts();
    QCOMPARE(pending.size(), 1);
    manager.restoreDrafts(pending);

    QCOMPARE(manager.count(), 2); // startup untitled + the restored buffer
    manager.setCurrentIndex(1);
    QCOMPARE(editor.text(), QStringLiteral("work in progress\n"));
    QVERIFY(manager.current()->isModified());
    QCOMPARE(manager.current()->draftId(), QStringLiteral("recover1"));
}

void TestDocumentManager::autosaveTimerWritesADraftOnItsOwn()
{
    QTemporaryDir drafts;
    QVERIFY(drafts.isValid());

    Editor editor;
    DocumentManager manager(&editor);
    manager.setAutosaveInterval(50);
    manager.setDraftDirectory(drafts.path());

    editor.setText(QStringLiteral("typed and left alone\n"));

    QTRY_VERIFY_WITH_TIMEOUT(!DraftStore(drafts.path()).loadAll().isEmpty(), 3000);
    QCOMPARE(DraftStore(drafts.path()).loadAll().first().text,
             QStringLiteral("typed and left alone\n"));
}

QTEST_MAIN(TestDocumentManager)
#include "test_document_manager.moc"
