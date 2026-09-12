#pragma once

#include <cstdint>
#include <limits>
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

/// Outcome of one parse, delivered back to the GUI thread. `spans` tile
/// [rangeStart, rangeEnd) exactly (see HighlightRange) — not necessarily the
/// whole document — in absolute document byte offsets, contiguous and in
/// order.
struct HighlightResult
{
    quint64 revision = 0;
    QString rootType;
    int namedChildCount = 0;
    bool ok = false;
    quint32 rangeStart = 0;
    quint32 rangeEnd = 0;
    QVector<HighlightSpan> spans;
};

/// The byte range to compute (and, on the GUI thread, paint) spans for.
/// Defaults to the whole document — every submit() overload that doesn't
/// pass one keeps computing full-document spans, exactly as before this
/// type existed. A real sub-range is how a keystroke's cost stays
/// independent of document size: only what changed and what's on screen
/// (plus a margin) needs a fresh pass; everything else keeps whatever
/// styling it already has, and Scintilla's own StyleNeeded notification
/// (see Editor::onNotify) asks for more, lazily, the moment the viewport
/// scrolls somewhere that hasn't been styled yet.
struct HighlightRange
{
    quint32 start = 0;
    quint32 end = std::numeric_limits<quint32>::max();
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
    /// `range` is unioned (min start, max end) across every submission in
    /// the interval, and only that final range is actually computed and
    /// returned in the result — see HighlightRange. `textChanged` false
    /// asserts the document is byte-identical to what's already parsed (a
    /// pure "show me more of what's already there" request, e.g. scrolling
    /// into unstyled territory): every submission in the interval skips
    /// reparsing entirely and spans are computed straight off the existing
    /// tree, using `text` only to detect the (should-never-happen) case
    /// where the caller was wrong. Every submission in the interval must
    /// agree on this or reparsing wins, since it is always safe and
    /// "nothing changed" is not. No defaults here — every real call goes
    /// through HighlightController, which is where the convenience of
    /// omitting these belongs.
    void submit(const QString& text, quint64 revision, PendingEdit edit, HighlightRange range,
                bool textChanged);

signals:
    void parsed(const hungryeditor::HighlightResult& result);

private:
    void runPendingParse();
    void clearQueries();
    /// Spans tiling exactly [rangeStart, min(rangeEnd, source.size())) —
    /// absolute document offsets, contiguous, in order.
    QVector<HighlightSpan> computeSpans(std::string_view source, quint32 rangeStart,
                                        quint32 rangeEnd) const;
    /// Paint every capture of every predicate-satisfying match of `query`
    /// under `root` that overlaps [rangeStart, rangeEnd) into `byteStyle`
    /// (sized to that range, index 0 == rangeStart), clipping any capture
    /// that only partially overlaps. Node offsets are shifted by
    /// `baseOffset` first (non-zero for injected sub-trees, where `root`'s
    /// own coordinates are relative to the injected snippet's text, not the
    /// outer document — `rangeStart`/`rangeEnd` stay in outer-document
    /// coordinates throughout, translated to `root`'s own space only for
    /// bounding the query cursor). `textForPredicates` is the text captured
    /// nodes' byte offsets (pre-baseOffset-shift) are relative to.
    void paintCaptures(TSQuery* query, const TSNode& root, quint32 baseOffset, quint32 rangeStart,
                       quint32 rangeEnd, std::string_view textForPredicates,
                       std::vector<qint32>& byteStyle) const;
    /// Walk the injection query — bounded to [rangeStart, rangeEnd), so a
    /// fenced block entirely outside it is never even sub-parsed — and paint
    /// each recognised sub-grammar over the bytes of its injection.content
    /// node. Each sub-grammar's own compiled query comes from
    /// GrammarRegistry's process-wide cache (shared with CodeHighlighter's
    /// preview rendering), not owned here.
    void paintInjections(std::string_view source, quint32 rangeStart, quint32 rangeEnd,
                         std::vector<qint32>& byteStyle) const;

    TreeSitterEngine engine_;
    TSQuery* query_ = nullptr;
    TSQuery* injectionQuery_ = nullptr;
    QTimer* debounce_ = nullptr;
    QString pendingText_;
    quint64 pendingRevision_ = 0;
    bool havePending_ = false;
    // Edits queued since the last parse, in the order submit() received
    // them, and the range those submissions (or a scroll-only request; see
    // HighlightRange) asked for, unioned. All reset at the start of
    // runPendingParse() — pendingEditsAllPresent_ back to true,
    // pendingTextChanged_ back to false (only a submission that actually
    // asserts a change should trigger a reparse), the range back to
    // empty — which is also where the accumulated batch is actually applied.
    std::vector<TSInputEdit> pendingEdits_;
    bool pendingEditsAllPresent_ = true;
    bool pendingTextChanged_ = false;
    quint32 pendingRangeStart_ = std::numeric_limits<quint32>::max();
    quint32 pendingRangeEnd_ = 0;
};

} // namespace hungryeditor

Q_DECLARE_METATYPE(hungryeditor::HighlightResult)
Q_DECLARE_METATYPE(hungryeditor::PendingEdit)
Q_DECLARE_METATYPE(hungryeditor::HighlightRange)
