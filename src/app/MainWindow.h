#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>
#include <QStringList>

class QAction;
class QDragEnterEvent;
class QDropEvent;

namespace hungryeditor {

class DocumentManager;
class Editor;
class TabBar;

/// The application's single top-level window. It hosts one editor widget
/// backed by a DocumentManager (multiple open buffers, switched in place),
/// a tab strip, a menu bar and open/save file handling. The preview pane is
/// added in a later phase.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// The editor widget filling the window. Exposed for tests.
    Editor* editor() const { return editor_; }

    /// The document tab strip. Exposed for tests.
    TabBar* tabBar() const { return tabBar_; }

    /// The open-document model. Exposed for tests.
    DocumentManager* documents() const { return documents_.get(); }

    /// Path backing the current buffer, or empty for an unsaved document.
    QString currentPath() const;

    /// Message from the last failed openPath()/savePath(), for tests and
    /// the dialog slots to surface.
    QString lastError() const { return lastError_; }

    /// Load `path` into a document, replacing an already-open one for the
    /// same path. Returns false on an I/O error (see lastError()).
    bool openPath(const QString& path);

    /// Open every path in `paths`, activating the first that loads. A pristine
    /// untitled buffer is dropped so command-line and drag-and-drop opens do
    /// not leave a stray tab. Returns false if any path failed; lastError()
    /// then holds one line per failure.
    bool openFiles(const QStringList& paths);

    /// Write the current document to `path` and mark it clean. Returns false
    /// on an I/O error (see lastError()).
    bool savePath(const QString& path);

    /// Close the document at `index`, prompting to discard unsaved changes.
    void closeDocumentAt(int index);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void showAbout();
    void newDocument();
    void openFileDialog();
    void save();
    void saveAsDialog();
    void closeCurrentDocument();
    void nextDocument();
    void previousDocument();

private:
    void buildMenus();
    void updateWindowTitle();

    // Tab strip <-> document model wiring.
    void primeTabs();
    void syncTabText(int index);
    void onDocumentAdded(int index);
    void onDocumentClosed(int index);
    void onCurrentChanged(int index);

    // The editor is owned by the QObject tree; the document manager is a
    // plain member so it is destroyed (releasing its Scintilla document
    // pointers through the editor) while the editor is still alive.
    Editor* editor_ = nullptr;
    TabBar* tabBar_ = nullptr;
    std::unique_ptr<DocumentManager> documents_;
    QAction* saveAction_ = nullptr;
    QString lastError_;
    bool syncingTabs_ = false;
    bool reorderingTabs_ = false;
};

} // namespace hungryeditor
