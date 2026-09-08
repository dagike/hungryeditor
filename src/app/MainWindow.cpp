#include "app/MainWindow.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>

#include "editor/Editor.h"
#include "io/TextFile.h"

#ifndef HUNGRYEDITOR_VERSION
#define HUNGRYEDITOR_VERSION "0.0.0"
#endif

namespace hungryeditor {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    resize(1000, 720);

    editor_ = new Editor(this);
    setCentralWidget(editor_);

    buildMenus();

    connect(editor_, &Editor::modifiedChanged, this, [this](bool modified) {
        saveAction_->setEnabled(modified);
        updateWindowTitle();
    });

    updateWindowTitle();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* openAction = fileMenu->addAction(tr("&Open…"), this, &MainWindow::openFileDialog);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setObjectName(QStringLiteral("action.open"));

    saveAction_ = fileMenu->addAction(tr("&Save"), this, &MainWindow::save);
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setObjectName(QStringLiteral("action.save"));
    saveAction_->setEnabled(false);

    QAction* saveAsAction = fileMenu->addAction(tr("Save &As…"), this, &MainWindow::saveAsDialog);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    saveAsAction->setObjectName(QStringLiteral("action.saveAs"));

    fileMenu->addSeparator();

    QAction* quitAction = fileMenu->addAction(tr("&Quit"), qApp, &QApplication::quit);
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);
    quitAction->setObjectName(QStringLiteral("action.quit"));

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction =
        helpMenu->addAction(tr("&About hungryeditor"), this, &MainWindow::showAbout);
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setObjectName(QStringLiteral("action.about"));
}

bool MainWindow::openPath(const QString& path)
{
    FileError error;
    const TextDocument doc = loadFile(path, &error);
    if (!error.ok) {
        lastError_ = error.message;
        return false;
    }

    editor_->setText(doc.text);
    editor_->setEncoding(doc.encoding);
    editor_->setLineEnding(doc.lineEnding);
    editor_->markClean();

    currentPath_ = path;
    lastError_.clear();
    saveAction_->setEnabled(false);
    updateWindowTitle();
    return true;
}

bool MainWindow::savePath(const QString& path)
{
    const TextDocument doc{editor_->text(), editor_->encoding(), editor_->lineEnding()};
    FileError error;
    if (!saveFile(path, doc, &error)) {
        lastError_ = error.message;
        return false;
    }

    editor_->markClean();
    currentPath_ = path;
    lastError_.clear();
    updateWindowTitle();
    return true;
}

void MainWindow::openFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open File"), currentPath_,
        tr("Markdown (*.md *.markdown *.mkd);;Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    if (!openPath(path)) {
        QMessageBox::warning(this, tr("Open Failed"), lastError_);
    }
}

void MainWindow::save()
{
    if (currentPath_.isEmpty()) {
        saveAsDialog();
        return;
    }
    if (!savePath(currentPath_)) {
        QMessageBox::warning(this, tr("Save Failed"), lastError_);
    }
}

void MainWindow::saveAsDialog()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save File As"), currentPath_,
        tr("Markdown (*.md *.markdown *.mkd);;Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    if (!savePath(path)) {
        QMessageBox::warning(this, tr("Save Failed"), lastError_);
    }
}

void MainWindow::updateWindowTitle()
{
    const QString name =
        currentPath_.isEmpty() ? tr("Untitled") : QFileInfo(currentPath_).fileName();
    const QString marker = editor_->isModified() ? QStringLiteral("*") : QString();
    setWindowTitle(QStringLiteral("%1%2 — hungryeditor").arg(marker, name));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About hungryeditor"),
                       tr("<h3>hungryeditor %1</h3>"
                          "<p>A fast, native Markdown editor.</p>")
                           .arg(QStringLiteral(HUNGRYEDITOR_VERSION)));
}

} // namespace hungryeditor
