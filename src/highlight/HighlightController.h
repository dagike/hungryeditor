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

    void setLanguage(const TSLanguage* language);

    /// Submit the current document text. Returns the revision assigned to
    /// this submission; results for superseded revisions are discarded.
    quint64 submit(const QString& text);

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
};

} // namespace hungryeditor
