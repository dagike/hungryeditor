#pragma once

#include <string_view>
#include <vector>

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

/// The single-range edit one Scintilla notification describes, or "none"
/// (`present == false`) when the caller has no such description for a
/// HighlightWorker::submit()/HighlightController::submit() call. Any
/// submission in a debounce window without one falls that whole window back
/// to a full reparse of its final text — always correct, just not the fast
/// path — so callers that can't describe an edit (or don't bother to) never
/// need special-casing. Namespace-scope, not nested in HighlightWorker: a
/// default argument referencing a slot's own enclosing class's nested type
/// doesn't compile on this toolchain, and Qt 6.4's QMetaObject::invokeMethod
/// has no member-function-pointer overload that forwards arguments (that
/// needs 6.5+), so the cross-thread call goes through the older
/// string+Q_ARG form — which matches by the exact spelling moc recorded,
/// so the type's name must read the same, unqualified, everywhere it's used.
struct PendingEdit
{
    bool present = false;
    TSInputEdit edit{};
};

/// Runs tree-sitter parsing and highlight-query evaluation off the GUI
/// thread. Lives on a worker QThread (see HighlightController); every slot
/// here executes on that thread.
///
/// Rapid edits are coalesced: every submission within one debounce interval
/// is queued, and the interval's single parse applies them all — see
/// PendingEdit and submit().
class HighlightWorker : public QObject
{
    Q_OBJECT

public:
    explicit HighlightWorker(QObject* parent = nullptr);
    ~HighlightWorker() override;

    static int debounceIntervalMs();

public slots:
    /// Set the grammar, its highlights.scm query source, and its
    /// injections.scm query source (may be empty). The injection query drives
    /// sub-grammar highlighting for fenced code blocks and inline spans.
    void configure(const TSLanguage* language, const QString& highlightQuery,
                   const QString& injectionQuery);
    /// Queue a parse at the given revision. `edit` describes the single-range
    /// change this submission's text reflects relative to the previous
    /// submission, if any; several edits queued within one debounce interval
    /// are folded into one incremental reparse (see TreeSitterEngine::
    /// noteEdit()/reparse()) as long as every one of them has an edit —
    /// otherwise the interval's parse falls back to a full reparse of `text`.
    /// No default here — every real call goes through HighlightController,
    /// which is where the convenience of omitting `edit` belongs.
    void submit(const QString& text, quint64 revision, PendingEdit edit);

signals:
    void parsed(const hungryeditor::HighlightResult& result);

private:
    void runPendingParse();
    void clearQueries();
    QVector<HighlightSpan> computeSpans(std::string_view source) const;
    /// Paint every capture of every predicate-satisfying match of `query`
    /// under `root` into `byteStyle`, shifting node offsets by `baseOffset`
    /// (non-zero for injected sub-trees). `textForPredicates` is the text
    /// captured nodes' byte offsets are relative to — the injected snippet's
    /// own text for a sub-tree, not the outer document.
    void paintCaptures(TSQuery* query, const TSNode& root, quint32 baseOffset,
                       std::string_view textForPredicates, std::vector<qint32>& byteStyle) const;
    /// Walk the injection query and paint each recognised sub-grammar over the
    /// bytes of its injection.content node. Each sub-grammar's own compiled
    /// query comes from GrammarRegistry's process-wide cache (shared with
    /// CodeHighlighter's preview rendering), not owned here.
    void paintInjections(std::string_view source, std::vector<qint32>& byteStyle) const;

    TreeSitterEngine engine_;
    TSQuery* query_ = nullptr;
    TSQuery* injectionQuery_ = nullptr;
    QTimer* debounce_ = nullptr;
    QString pendingText_;
    quint64 pendingRevision_ = 0;
    bool havePending_ = false;
    // Edits queued since the last parse, in the order submit() received
    // them. Cleared — and pendingEditsAllPresent_ reset to true — at the
    // start of runPendingParse(), which is also where the accumulated batch
    // is actually applied.
    std::vector<TSInputEdit> pendingEdits_;
    bool pendingEditsAllPresent_ = true;
};

} // namespace hungryeditor

Q_DECLARE_METATYPE(hungryeditor::HighlightResult)
Q_DECLARE_METATYPE(hungryeditor::PendingEdit)
