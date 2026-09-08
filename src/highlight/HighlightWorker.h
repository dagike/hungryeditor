#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QVector>

#include <tree_sitter/api.h>

#include "highlight/TreeSitterEngine.h"

class QTimer;

// TSLanguage is an opaque struct; allow it to travel through queued signal
// and slot connections as a bare pointer.
Q_DECLARE_OPAQUE_POINTER(const TSLanguage*)

namespace hungryeditor {

/// A run of bytes that should be painted with one Scintilla style.
struct HighlightSpan
{
    quint32 start = 0;
    quint32 length = 0;
    qint32 style = 0;
};

/// Outcome of one parse, delivered back to the GUI thread.
struct HighlightResult
{
    quint64 revision = 0;
    QString rootType;
    int namedChildCount = 0;
    bool ok = false;
    QVector<HighlightSpan> spans;
};

/// Runs tree-sitter parsing and highlight-query evaluation off the GUI
/// thread. Lives on a worker QThread (see HighlightController); every slot
/// here executes on that thread.
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
    /// Set the grammar and its highlights.scm query source.
    void configure(const TSLanguage* language, const QString& highlightQuery);
    /// Queue a full-document parse at the given revision.
    void submit(const QString& text, quint64 revision);

signals:
    void parsed(const hungryeditor::HighlightResult& result);

private:
    void runPendingParse();
    QVector<HighlightSpan> computeSpans(std::string_view source) const;

    TreeSitterEngine engine_;
    TSQuery* query_ = nullptr;
    QTimer* debounce_ = nullptr;
    QString pendingText_;
    quint64 pendingRevision_ = 0;
    bool havePending_ = false;
};

} // namespace hungryeditor

Q_DECLARE_METATYPE(hungryeditor::HighlightResult)
