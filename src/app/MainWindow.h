#pragma once

#include <memory>

#include <QList>
#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QStringList>

#include "editor/CaretHistory.h"
#include "theme/Theme.h"

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
class FileTreePanel;
class FindReplaceBar;
class OutlinePanel;
class PreviewBackend;
class PreviewController;
class RecentFiles;
class SearchResultsPanel;
class SessionStore;
class TabBar;
class TabSwitcher;
class WorkspaceStore;

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

    /// The bundled palette last explicitly picked (kept as a fallback while a
    /// custom theme is active; see currentTheme()).
    Theme::Builtin currentBuiltinTheme() const { return currentTheme_; }
    /// The colours actually in effect: a loaded custom theme's, if any,
    /// otherwise the selected builtin's.
    Theme currentTheme() const
    {
        return customThemePath_.isEmpty() ? Theme::forBuiltin(currentTheme_) : customTheme_;
    }
    /// Switch the preview (and the persisted session) to the builtin `id`,
    /// clearing any active custom theme.
    void setTheme(Theme::Builtin id);

    /// Load a JSON theme file (see theme/ThemeFile.h) and apply it to the
    /// preview, superseding the builtin selection until a builtin is chosen
    /// again. Returns false on a read/parse failure (see lastError()),
    /// leaving the current theme unchanged.
    bool loadCustomTheme(const QString& path);

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

    /// Open buffers in most-recently-used order (front is current), by display
    /// name. Exposed for tests.
    QStringList mruDocumentNames() const;

    /// Path backing the current buffer, or empty for an unsaved document.
    QString currentPath() const;

    /// Message from the last failed openPath()/savePath(), for tests and
    /// the dialog slots to surface.
    QString lastError() const { return lastError_; }

    /// Load `path` into a document, replacing an already-open one for the
    /// same path. Returns false on an I/O error (see lastError()).
    bool openPath(const QString& path);

    /// Move the caret to `oneBasedLine`, clamped to the buffer, recording the
    /// spot left for Back/Forward navigation. Exposed for tests.
    void goToLine(int oneBasedLine);

    /// Pin the folder sidebar to `dir` (revealing it) and remember that
    /// workspace's view state; an empty `dir` closes the folder and lets the
    /// sidebar follow the current document again.
    void openFolder(const QString& dir);

    /// The explicitly opened workspace folder, or empty when none is open.
    QString workspaceFolder() const { return workspaceRoot_; }

    /// Sidebar file operations. Each performs the filesystem change, refreshes
    /// the tree and reconciles any open buffer, returning false (with
    /// lastError() set) on failure. The private slots wrap these with the
    /// name/confirm dialogs; tests call them directly.
    bool createFileInWorkspace(const QString& parentDir, const QString& name);
    bool createFolderInWorkspace(const QString& parentDir, const QString& name);
    bool renameInWorkspace(const QString& path, const QString& newName);
    bool deleteFromWorkspace(const QString& path);

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

    /// Title an export of the current buffer should carry: its first heading,
    /// else its display name (without extension), else "Untitled". Exposed
    /// for tests.
    QString exportTitle() const;

    /// The current buffer rendered as a standalone HTML document, per
    /// htmlexport::build(). Exposed for tests.
    QString buildHtmlExport() const;

    /// Write buildHtmlExport() to `path`. Returns false on an I/O error (see
    /// lastError()).
    bool exportHtmlTo(const QString& path);

    /// Render the current buffer's preview to a standalone PDF at `path`.
    /// Forces a preview render first, even in Editor-only view. Returns
    /// false on failure (see lastError()).
    bool exportPdfTo(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void showAbout();
    void newDocument();
    void openFileDialog();
    void openFolderDialog();
    void save();
    void saveAsDialog();
    void closeCurrentDocument();
    void nextDocument();
    void previousDocument();
    void exportHtmlDialog();
    void printDialog();
    void exportPdfDialog();
    void copyAsRichText();
    void loadCustomThemeDialog();

private:
    void buildMenus();
    void updateWindowTitle();

    // Live preview: created on construction, fed the editor's text (debounced)
    // whenever a preview pane is visible.
    void applyViewMode();
    void refreshPreview();
    // Forces a render even in Editor-only view (refreshPreview() skips it
    // there) and blocks until the preview shell is up, for Print/PDF export.
    void ensurePreviewRendered();

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

    // Most-recently-used buffer order, for Ctrl+Tab switching and for listing
    // open buffers ahead of the file index in "Go to Anything".
    void reconcileMru();
    // Step through / raise the Ctrl+Tab overlay. `direction` is +1 forward
    // (older), -1 backward.
    void quickSwitch(int direction);

    // Go to line and Back/Forward caret navigation.
    void goToLineDialog();
    void navigateBack();
    void navigateForward();
    void recordCaretForHistory();
    CaretLocation currentLocation() const;
    void applyLocation(const CaretLocation& location);

    // Folder sidebar: point it (and the shared file index) at the open
    // workspace folder, or the current document's directory when none is
    // pinned — but only while the sidebar is switched on.
    void updateWorkspaceRoot();
    // Load / persist the open workspace's remembered filter and tree state.
    void loadWorkspaceViewState();
    void saveWorkspaceViewState();
    // Indices of open buffers whose file is `path` (or, when `path` is a
    // directory, sits under it), current-first.
    QList<int> documentsAffectedBy(const QString& path) const;
    // Sidebar context-menu handlers: prompt, then call the public do-ers.
    void promptCreateFile(const QString& parentDir);
    void promptCreateFolder(const QString& parentDir);
    void promptRename(const QString& path, bool isDirectory);
    void promptDelete(const QString& path, bool isDirectory);

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
    TabSwitcher* tabSwitcher_ = nullptr;
    QList<Document*> mruDocuments_; ///< open buffers, most-recently-used first
    CaretHistory caretHistory_;
    bool navigatingHistory_ = false; ///< suppresses recording while Back/Forward runs
    FileIndex* fileIndex_ = nullptr;
    bool paletteShowsFiles_ = false;
    FileTreePanel* fileTree_ = nullptr;
    QDockWidget* fileTreeDock_ = nullptr;
    QAction* closeFolderAction_ = nullptr;
    QString workspaceRoot_; ///< explicitly opened folder, empty for none
    SearchResultsPanel* searchResults_ = nullptr;
    QDockWidget* searchDock_ = nullptr;
    OutlinePanel* outline_ = nullptr;
    QDockWidget* outlineDock_ = nullptr;
    QTimer* outlineTimer_ = nullptr;
    QSplitter* splitter_ = nullptr;
    std::unique_ptr<DocumentManager> documents_;
    std::unique_ptr<SessionStore> sessionStore_;
    std::unique_ptr<WorkspaceStore> workspaceStore_;
    std::unique_ptr<RecentFiles> recentFiles_;
    // previewController_ is declared after preview_ so it is torn down first —
    // it holds a raw pointer to the backend.
    std::unique_ptr<PreviewBackend> preview_;
    std::unique_ptr<PreviewController> previewController_;
    bool previewReady_ = false; ///< latched true once the preview shell first comes up
    QAction* saveAction_ = nullptr;
    QAction* foldFrontMatterAction_ = nullptr;
    QMenu* recentMenu_ = nullptr;
    QActionGroup* viewModeGroup_ = nullptr;
    ViewMode viewMode_ = ViewMode::Split;
    QActionGroup* themeGroup_ = nullptr;
    Theme::Builtin currentTheme_ = Theme::Builtin::Light;
    QString customThemePath_; ///< empty when no custom theme is active
    Theme customTheme_;
    QString customThemeCss_;
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
