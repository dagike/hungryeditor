#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <tree_sitter/api.h>

#include "highlight/HighlightWorker.h"

class QThread;

namespace hungryeditor {

/// GUI-thread handle for background highlighting. Owns the worker thread,
/// forwards document text to the worker, and re-emits results on the GUI
/// thread. One instance per editor.
class HighlightController : public QObject
{
    Q_OBJECT

public:
    explicit HighlightController(QObject* parent = nullptr);
    ~HighlightController() override;

    /// Set the grammar, its highlights.scm query text, and its injections.scm
    /// query text (empty to disable sub-grammar highlighting).
    void configure(const TSLanguage* language, const QString& highlightQuery,
                   const QString& injectionQuery = {});

    /// Submit the current document text. Returns the revision assigned to
    /// this submission; results for superseded revisions are discarded.
    /// A no-op (returns the current revision) while disabled.
    quint64 submit(const QString& text);

    /// Enable or disable parsing. Disabled by the editor's large-file
    /// fallback: submissions are ignored and no results are emitted until
    /// re-enabled.
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

    quint64 currentRevision() const { return revision_; }
    quint64 lastResultRevision() const { return lastResultRevision_; }

    /// The thread the parser runs on. Exposed for tests.
    QThread* workerThread() const { return thread_; }

signals:
    void highlighted(const hungryeditor::HighlightResult& result);

private:
    void onParsed(const HighlightResult& result);

    QThread* thread_ = nullptr;
    HighlightWorker* worker_ = nullptr;
    quint64 revision_ = 0;
    quint64 lastResultRevision_ = 0;
    bool enabled_ = true;
};

} // namespace hungryeditor
