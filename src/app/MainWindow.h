#pragma once

#include <QMainWindow>
#include <QString>

class QAction;

namespace hungryeditor {

class Editor;

/// The application's single top-level window. It hosts a bare editor widget,
/// a minimal menu bar and open/save file handling; tabs and the preview pane
/// are added in later phases.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// The editor widget filling the window. Exposed for tests.
    Editor* editor() const { return editor_; }

    /// Path backing the current buffer, or empty for an unsaved document.
    QString currentPath() const { return currentPath_; }

    /// Message from the last failed openPath()/savePath(), for tests and
    /// the dialog slots to surface.
    QString lastError() const { return lastError_; }

    /// Load `path` into the editor, replacing the current buffer. Returns
    /// false on an I/O error (see lastError()). Exposed for tests and for
    /// command-line / drag-and-drop callers in later phases.
    bool openPath(const QString& path);

    /// Write the current buffer to `path` and mark it clean. Returns false
    /// on an I/O error (see lastError()).
    bool savePath(const QString& path);

private slots:
    void showAbout();
    void openFileDialog();
    void save();
    void saveAsDialog();

private:
    void buildMenus();
    void updateWindowTitle();

    Editor* editor_ = nullptr;
    QAction* saveAction_ = nullptr;
    QString currentPath_;
    QString lastError_;
};

} // namespace hungryeditor
