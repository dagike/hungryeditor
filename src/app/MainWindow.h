#pragma once

#include <memory>

#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QStringList>

class QAction;
class QActionGroup;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QMenu;
class QSplitter;
class QTimer;

namespace hungryeditor {

class CommandPalette;
class Document;
class DocumentManager;
class Editor;
class FileIndex;
class FindReplaceBar;
class OutlinePanel;
class PreviewBackend;
class PreviewController;
class RecentFiles;
class SearchResultsPanel;
class SessionStore;
class TabBar;

/// The application's single top-level window. It hosts one editor widget
/// backed by a DocumentManager (multiple open buffers, switched in place),
/// a tab strip, a menu bar, open/save file handling and a live HTML preview
/// beside the editor.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Which panes the window shows.
    enum class ViewMode
    {
        Editor,  ///< editor only
        Split,   ///< editor and preview side by side
        Preview, ///< preview only
    };

    ViewMode viewMode() const { return viewMode_; }
    void setViewMode(ViewMode mode);

    /// The debounce-and-render controller feeding the preview. Exposed for tests.
    PreviewController* previewController() const { return previewController_.get(); }

    /// The preview rendering backend. Exposed for tests.
    PreviewBackend* previewBackend() const { return preview_.get(); }

    /// The preview pane widget, for tests to check visibility.
    QWidget* previewWidget() const;

    /// The find-in-files results panel. Exposed for tests.
    SearchResultsPanel* searchResultsPanel() const { return searchResults_; }

    /// The editor widget filling the window. Exposed for tests.
    Editor* editor() const { return editor_; }

    /// The document tab strip. Exposed for tests.
    TabBar* tabBar() const { return tabBar_; }

    /// The "Open Recent" submenu. Exposed for tests.
    QMenu* recentFilesMenu() const { return recentMenu_; }

    /// Repopulate the "Open Recent" submenu from the stored list. Normally run
    /// from the menu's aboutToShow; exposed for tests.
    void refreshRecentFilesMenu();

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

    /// Re-read the document at `index` from disk. Returns false on an I/O
    /// error (see lastError()). Exposed for tests and the change-on-disk
    /// handling.
    bool reloadDocumentAt(int index);

    /// True when a previous session left recovery drafts behind (it did not
    /// exit cleanly).
    bool hasRecoverableDrafts() const;

    /// Recreate the buffers a previous session left as recovery drafts. With
    /// `askFirst` the user is prompted; declining discards the drafts.
    void restoreUnsavedFromLastSession(bool askFirst = true);

    /// Point recovery drafts and the session file at `directory` (drafts in a
    /// `drafts/` subdirectory, the session in `session.json`). Called with the
    /// app data location on construction; overridden by tests.
    void setStateDirectory(const QString& directory);

    /// Restore the tabs, caret positions and window geometry left by the last
    /// clean exit. With no session file this falls back to crash recovery
    /// (restoreUnsavedFromLastSession), forwarding `askFirst`.
    void restoreLastSession(bool askFirst = true);

    /// Write the current tabs, carets and geometry to the session file. Run
    /// from the aboutToQuit hook; exposed for tests.
    void saveSession();

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

    // Live preview: created on construction, fed the editor's text (debounced)
    // whenever a preview pane is visible.
    void applyViewMode();
    void refreshPreview();

    // Scroll sync. Each direction guards against the echo the other would cause.
    void syncPreviewToEditor();
    void syncEditorToPreview(int line);

    // Clicking a heading in the preview drops the editor caret on its source.
    void jumpEditorToLine(int line);

    // Find / replace bar and the find-in-files panel.
    void refreshFindHighlight();
    void closeFindBar();
    void findInFiles();

    // Command palette and quick open (share one widget).
    void openCommandPalette();
    void openQuickOpen();
    void populateQuickOpen();
    void runPaletteChoice(const QString& id);

    // Recent-files list and its menu.
    void recordRecent(const QString& path);
    void openRecent(const QString& path);

    // Tab strip <-> document model wiring.
    void primeTabs();
    void dropInitialBlankBuffer();
    void syncTabText(int index);
    void onDocumentAdded(int index);
    void onDocumentClosed(int index);
    void onCurrentChanged(int index);
    /// Enable / check the "Fold Front Matter" action for the current buffer.
    void updateFrontMatterAction();
    /// Re-extract the heading outline for the current buffer and re-select the
    /// entry the caret sits under.
    void rebuildOutline();

    // External file-change handling.
    void onFileChangedExternally(int index);
    void onFileRemovedExternally(int index);
    bool confirmReloadOverLocalChanges(const QString& name);

    // The editor is owned by the QObject tree; the document manager is a
    // plain member so it is destroyed (releasing its Scintilla document
    // pointers through the editor) while the editor is still alive.
    Editor* editor_ = nullptr;
    TabBar* tabBar_ = nullptr;
    FindReplaceBar* findBar_ = nullptr;
    CommandPalette* commandPalette_ = nullptr;
    FileIndex* fileIndex_ = nullptr;
    bool paletteShowsFiles_ = false;
    SearchResultsPanel* searchResults_ = nullptr;
    QDockWidget* searchDock_ = nullptr;
    OutlinePanel* outline_ = nullptr;
    QDockWidget* outlineDock_ = nullptr;
    QTimer* outlineTimer_ = nullptr;
    QSplitter* splitter_ = nullptr;
    std::unique_ptr<DocumentManager> documents_;
    std::unique_ptr<SessionStore> sessionStore_;
    std::unique_ptr<RecentFiles> recentFiles_;
    // previewController_ is declared after preview_ so it is torn down first —
    // it holds a raw pointer to the backend.
    std::unique_ptr<PreviewBackend> preview_;
    std::unique_ptr<PreviewController> previewController_;
    QAction* saveAction_ = nullptr;
    QAction* foldFrontMatterAction_ = nullptr;
    QMenu* recentMenu_ = nullptr;
    QActionGroup* viewModeGroup_ = nullptr;
    ViewMode viewMode_ = ViewMode::Split;
    QString lastError_;
    QString stateDir_;
    bool syncingTabs_ = false;
    bool reorderingTabs_ = false;
    bool syncingScroll_ = false;

    // Documents with a reload prompt already queued for the next event-loop
    // turn, and files whose disappearance has already been reported — so
    // neither warning stacks up while the watcher keeps firing.
    QSet<Document*> pendingReloadPrompts_;
    QSet<QString> reportedMissingFiles_;
};

} // namespace hungryeditor
