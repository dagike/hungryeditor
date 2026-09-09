#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>

class QAction;

namespace hungryeditor {

class DocumentManager;
class Editor;

/// The application's single top-level window. It hosts one editor widget
/// backed by a DocumentManager (multiple open buffers, switched in place),
/// a menu bar and open/save file handling. The visible tab bar and the
/// preview pane are added in later phases.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// The editor widget filling the window. Exposed for tests.
    Editor* editor() const { return editor_; }

    /// The open-document model. Exposed for tests.
    DocumentManager* documents() const { return documents_.get(); }

    /// Path backing the current buffer, or empty for an unsaved document.
    QString currentPath() const;

    /// Message from the last failed openPath()/savePath(), for tests and
    /// the dialog slots to surface.
    QString lastError() const { return lastError_; }

    /// Load `path` into a document, replacing an already-open one for the
    /// same path. Returns false on an I/O error (see lastError()). Exposed
    /// for tests and for command-line / drag-and-drop callers in later phases.
    bool openPath(const QString& path);

    /// Write the current document to `path` and mark it clean. Returns false
    /// on an I/O error (see lastError()).
    bool savePath(const QString& path);

private slots:
    void showAbout();
    void newDocument();
    void openFileDialog();
    void save();
    void saveAsDialog();
    void nextDocument();
    void previousDocument();

private:
    void buildMenus();
    void updateWindowTitle();

    // The editor is owned by the QObject tree; the document manager is a
    // plain member so it is destroyed (releasing its Scintilla document
    // pointers through the editor) while the editor is still alive.
    Editor* editor_ = nullptr;
    std::unique_ptr<DocumentManager> documents_;
    QAction* saveAction_ = nullptr;
    QString lastError_;
};

} // namespace hungryeditor
