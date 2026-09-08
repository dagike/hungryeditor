#include "app/MainWindow.h"

#include <QApplication>
#include <QMenuBar>
#include <QMessageBox>

#include <ScintillaEditBase.h>

#ifndef HUNGRYEDITOR_VERSION
#define HUNGRYEDITOR_VERSION "0.0.0"
#endif

namespace hungryeditor
{

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("hungryeditor"));
    resize(1000, 720);

    editor_ = new ScintillaEditBase(this);
    setCentralWidget(editor_);

    buildMenus();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* quitAction = fileMenu->addAction(tr("&Quit"), qApp, &QApplication::quit);
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);
    quitAction->setObjectName(QStringLiteral("action.quit"));

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction = helpMenu->addAction(tr("&About hungryeditor"), this, &MainWindow::showAbout);
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setObjectName(QStringLiteral("action.about"));
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About hungryeditor"),
        tr("<h3>hungryeditor %1</h3>"
           "<p>A fast, native Markdown editor.</p>")
            .arg(QStringLiteral(HUNGRYEDITOR_VERSION)));
}

} // namespace hungryeditor
