#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <tree_sitter/api.h>

#include "highlight/TreeSitterEngine.h"

class QTimer;

// TSLanguage is an opaque struct; allow it to travel through queued signal
// and slot connections as a bare pointer.
Q_DECLARE_OPAQUE_POINTER(const TSLanguage*)

namespace hungryeditor {

/// Outcome of one parse, delivered back to the GUI thread. The payload is
/// intentionally small for now; the capture -> style spans that actually
/// drive colouring are added in the next commit.
struct HighlightResult
{
    quint64 revision = 0;
    QString rootType;
    int namedChildCount = 0;
    bool ok = false;
};

/// Runs tree-sitter parsing off the GUI thread. Lives on a worker QThread
/// (see HighlightController); every slot here executes on that thread.
///
/// Rapid edits are coalesced: only the most recent submission is parsed once
/// the debounce interval elapses, so a burst of keystrokes costs one parse
/// and intermediate revisions are never processed.
class HighlightWorker : public QObject
{
    Q_OBJECT

public:
    explicit HighlightWorker(QObject* parent = nullptr);
    ~HighlightWorker() override;

    static int debounceIntervalMs();

public slots:
    void setLanguage(const TSLanguage* language);
    /// Queue a full-document parse at the given revision.
    void submit(const QString& text, quint64 revision);

signals:
    void parsed(const hungryeditor::HighlightResult& result);

private:
    void runPendingParse();

    TreeSitterEngine engine_;
    QTimer* debounce_ = nullptr;
    QString pendingText_;
    quint64 pendingRevision_ = 0;
    bool havePending_ = false;
};

} // namespace hungryeditor

Q_DECLARE_METATYPE(hungryeditor::HighlightResult)
