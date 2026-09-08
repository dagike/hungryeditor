#pragma once

#include <QMainWindow>

namespace hungryeditor {

class Editor;

/// The application's single top-level window. For now it hosts a bare editor
/// widget and a minimal menu bar; file handling, tabs and the preview pane
/// are added in later phases.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// The editor widget filling the window. Exposed for tests.
    Editor* editor() const { return editor_; }

private slots:
    void showAbout();

private:
    void buildMenus();

    Editor* editor_ = nullptr;
};

} // namespace hungryeditor
