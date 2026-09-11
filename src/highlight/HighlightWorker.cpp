#include "highlight/HighlightWorker.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <QByteArray>
#include <QTimer>

#include "highlight/CaptureStyles.h"
#include "highlight/GrammarRegistry.h"
#include "highlight/QueryPredicates.h"
#include "HighlightQueries.h" // generated: hungryeditor::queries::*

extern "C" const TSLanguage* tree_sitter_markdown_inline(void);

namespace hungryeditor {

namespace {
constexpr int kDebounceMs = 15;

TSQuery* newQuery(const TSLanguage* language, const QByteArray& scm)
{
    if (language == nullptr || scm.isEmpty()) {
        return nullptr;
    }
    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    // A malformed query simply disables that layer of highlighting; not fatal.
    return ts_query_new(language, scm.constData(), static_cast<uint32_t>(scm.size()), &errorOffset,
                        &errorType);
}

/// Resolve an injection language name to a grammar: the markdown-inline
/// sub-grammar for prose spans, otherwise a fenced-code language from the
/// registry (Rust, C, Python, ...).
Grammar injectedGrammar(std::string_view name)
{
    if (name == "markdown_inline" || name == "markdown.inline") {
        return {tree_sitter_markdown_inline(), queries::kMarkdownInlineHighlights};
    }
    return grammarForName(name);
}

} // namespace

HighlightWorker::HighlightWorker(QObject* parent) : QObject(parent)
{
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(kDebounceMs);
    connect(debounce_, &QTimer::timeout, this, &HighlightWorker::runPendingParse);
}

HighlightWorker::~HighlightWorker()
{
    clearQueries();
}

int HighlightWorker::debounceIntervalMs()
{
    return kDebounceMs;
}

void HighlightWorker::clearQueries()
{
    // query_/injectionQuery_ are markdown's own top-level queries, compiled
    // directly and owned here. Per-language sub-grammar queries come from
    // the shared cache (see cachedHighlightsQuery()) and outlive this
    // instance, so there is nothing else to release.
    if (query_ != nullptr) {
        ts_query_delete(query_);
        query_ = nullptr;
    }
    if (injectionQuery_ != nullptr) {
        ts_query_delete(injectionQuery_);
        injectionQuery_ = nullptr;
    }
}

void HighlightWorker::configure(const TSLanguage* language, const QString& highlightQuery,
                                const QString& injectionQuery)
{
    engine_.setLanguage(language);
    clearQueries();
    if (language == nullptr) {
        return;
    }

    query_ = newQuery(language, highlightQuery.toUtf8());
    injectionQuery_ = newQuery(language, injectionQuery.toUtf8());
}

void HighlightWorker::submit(const QString& text, quint64 revision, PendingEdit edit,
                             HighlightRange range, bool textChanged)
{
    pendingText_ = text;
    pendingRevision_ = revision;
    havePending_ = true;
    if (edit.present) {
        pendingEdits_.push_back(edit.edit);
    } else if (textChanged) {
        // Content changed, but this particular submission has no edit
        // description for it — the whole batch can only be described by a
        // full reparse, so stop bothering to accumulate. A pure
        // range-extension request (textChanged false, e.g. a scroll)
        // asserts nothing changed at all, so it has no bearing on this —
        // letting it poison an otherwise-clean edit chain coalesced into
        // the same debounce window would force a needless full reparse.
        pendingEditsAllPresent_ = false;
    }
    pendingTextChanged_ = pendingTextChanged_ || textChanged;
    pendingRangeStart_ = std::min(pendingRangeStart_, range.start);
    pendingRangeEnd_ = std::max(pendingRangeEnd_, range.end);
    debounce_->start(); // restart: coalesce bursts into one parse
}

void HighlightWorker::runPendingParse()
{
    if (!havePending_) {
        return;
    }
    havePending_ = false;

    const quint64 revision = pendingRevision_;
    const std::string source = pendingText_.toStdString();
    pendingText_.clear();

    // Every edit since the last parse, still in order — or none, if any
    // submission in this batch had no edit of its own (see submit()).
    const bool haveEdits = pendingEditsAllPresent_ && !pendingEdits_.empty();
    const std::vector<TSInputEdit> edits = std::move(pendingEdits_);
    pendingEdits_.clear();
    pendingEditsAllPresent_ = true;

    const bool anyContentChanged = pendingTextChanged_;
    pendingTextChanged_ = false;

    quint32 rangeStart = pendingRangeStart_;
    quint32 rangeEnd = pendingRangeEnd_;
    pendingRangeStart_ = std::numeric_limits<quint32>::max();
    pendingRangeEnd_ = 0;
    if (rangeStart > rangeEnd) {
        // Defensive only: submit() always narrows this via min/max, so it
        // can't actually be inverted for a batch that called submit() at
        // least once, which runPendingParse() firing at all guarantees.
        rangeStart = 0;
        rangeEnd = std::numeric_limits<quint32>::max();
    }

    // Which text computeSpans() below reads bytes from. Usually `source`
    // (this submission's text, just reparsed against); for a pure
    // range-extension request (anyContentChanged false — nothing actually
    // changed, see HighlightRange) the existing tree is reused untouched, so
    // spans must come from what that tree was actually built from, not from
    // trusting this submission's own `text` to still match it.
    std::string_view sourceForSpans = source;
    if (anyContentChanged) {
        if (haveEdits) {
            // Cheap bookkeeping per edit (shifts the existing tree's node
            // ranges; no-op if there is no tree yet), then one real reparse
            // against the final text — tree-sitter's own pattern for
            // folding several edits into a single incremental parse.
            for (const TSInputEdit& edit : edits) {
                engine_.noteEdit(edit);
            }
            engine_.reparse(source);
        } else {
            engine_.setText(source);
        }
    } else {
        sourceForSpans = engine_.source();
    }

    HighlightResult result;
    result.revision = revision;
    if (engine_.hasTree()) {
        const TSNode root = engine_.rootNode();
        result.rootType = QString::fromUtf8(ts_node_type(root));
        result.namedChildCount = static_cast<int>(ts_node_named_child_count(root));
        result.ok = true;
        const auto sourceSize = static_cast<quint32>(sourceForSpans.size());
        result.rangeEnd = std::min(rangeEnd, sourceSize);
        result.rangeStart = std::min(rangeStart, result.rangeEnd);
        result.spans = computeSpans(sourceForSpans, result.rangeStart, result.rangeEnd);
    }
    emit parsed(result);
}

QVector<HighlightSpan> HighlightWorker::computeSpans(std::string_view source, quint32 rangeStart,
                                                     quint32 rangeEnd) const
{
    QVector<HighlightSpan> spans;
    if (!engine_.hasTree() || rangeStart >= rangeEnd) {
        return spans;
    }

    // Paint a per-byte style buffer sized to just the requested range, then
    // run-length encode it. Later captures (which tree-sitter yields in
    // node order, more specific ones last) win; injected sub-grammars are
    // painted last and override the block layer.
    std::vector<qint32> byteStyle(rangeEnd - rangeStart, StylePlain);
    if (query_ != nullptr) {
        paintCaptures(query_, engine_.rootNode(), 0, rangeStart, rangeEnd, source, byteStyle);
    }
    paintInjections(source, rangeStart, rangeEnd, byteStyle);

    for (uint32_t i = 0; i < byteStyle.size();) {
        const qint32 style = byteStyle[i];
        uint32_t j = i + 1;
        while (j < byteStyle.size() && byteStyle[j] == style) {
            ++j;
        }
        spans.push_back(HighlightSpan{rangeStart + i, j - i, style});
        i = j;
    }
    return spans;
}

void HighlightWorker::paintCaptures(TSQuery* query, const TSNode& root, quint32 baseOffset,
                                    quint32 rangeStart, quint32 rangeEnd,
                                    std::string_view textForPredicates,
                                    std::vector<qint32>& byteStyle) const
{
    TSQueryCursor* cursor = ts_query_cursor_new();

    // Bound the walk to root's own coordinate space: baseOffset shifts
    // root's node offsets into the outer document's, so subtracting it back
    // translates the outer-document range into root's space. Saturating —
    // an injected sub-tree can start after rangeStart or end before
    // rangeEnd — so the cursor never sees a negative/wrapped bound. Must be
    // set before exec(), not after.
    const quint32 cursorStart = rangeStart > baseOffset ? rangeStart - baseOffset : 0;
    const quint32 cursorEnd = rangeEnd > baseOffset ? rangeEnd - baseOffset : 0;
    ts_query_cursor_set_byte_range(cursor, cursorStart, cursorEnd);
    ts_query_cursor_exec(cursor, query, root);

    // Matched (not flattened via next_capture) so a #match?/#eq?/#any-of?
    // predicate can see every capture of its own match — see
    // QueryPredicates.h. All of a satisfying match's captures are then
    // painted together, in the match's own capture order.
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        if (!predicates::matchesPredicates(query, match, textForPredicates)) {
            continue;
        }
        for (uint16_t i = 0; i < match.capture_count; ++i) {
            const TSQueryCapture& capture = match.captures[i];

            uint32_t nameLen = 0;
            const char* name = ts_query_capture_name_for_id(query, capture.index, &nameLen);
            const int style = styleForCapture(std::string_view(name, nameLen));
            if (style == StylePlain) {
                continue;
            }

            const quint32 start = baseOffset + ts_node_start_byte(capture.node);
            const quint32 end = baseOffset + ts_node_end_byte(capture.node);
            // A capture the byte-range restriction let through can still
            // straddle its boundary; clip to it before translating into
            // byteStyle's own range-relative indices.
            const quint32 clippedStart = std::max(start, rangeStart);
            const quint32 clippedEnd = std::min(end, rangeEnd);
            for (quint32 j = clippedStart; j < clippedEnd; ++j) {
                byteStyle[j - rangeStart] = style;
            }
        }
    }
    ts_query_cursor_delete(cursor);
}

void HighlightWorker::paintInjections(std::string_view source, quint32 rangeStart, quint32 rangeEnd,
                                      std::vector<qint32>& byteStyle) const
{
    if (injectionQuery_ == nullptr) {
        return;
    }

    TSQueryCursor* cursor = ts_query_cursor_new();
    // engine_.rootNode()'s coordinates are already outer-document-absolute,
    // so the range applies directly — this is what skips sub-parsing (and
    // sub-highlighting, a whole second grammar's query) a fenced block that
    // is nowhere near what's being computed. Must be set before exec().
    ts_query_cursor_set_byte_range(cursor, rangeStart, rangeEnd);
    ts_query_cursor_exec(cursor, injectionQuery_, engine_.rootNode());

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        TSNode contentNode{};
        bool haveContent = false;
        std::string language;

        for (uint16_t i = 0; i < match.capture_count; ++i) {
            const TSQueryCapture& capture = match.captures[i];
            uint32_t nameLen = 0;
            const char* name =
                ts_query_capture_name_for_id(injectionQuery_, capture.index, &nameLen);
            const std::string_view captureName(name, nameLen);

            if (captureName == "injection.content") {
                contentNode = capture.node;
                haveContent = true;
            } else if (captureName == "injection.language") {
                const uint32_t start = ts_node_start_byte(capture.node);
                const uint32_t end = ts_node_end_byte(capture.node);
                if (end <= source.size() && start < end) {
                    language.assign(source.substr(start, end - start));
                }
            }
        }
        if (!haveContent) {
            continue;
        }

        const uint32_t contentStart = ts_node_start_byte(contentNode);
        const uint32_t contentEnd = ts_node_end_byte(contentNode);
        if (contentEnd > source.size() || contentEnd <= contentStart) {
            continue;
        }
        // The byte-range restriction above can still admit a match where
        // only some other capture (e.g. the info_string) overlaps, not the
        // content node itself — skip a sub-parse that would paint nothing
        // inside [rangeStart, rangeEnd) anyway.
        if (contentEnd <= rangeStart || contentStart >= rangeEnd) {
            continue;
        }

        if (language.empty()) {
            language = predicates::directiveLanguage(injectionQuery_, match.pattern_index);
        }

        const Grammar grammar = injectedGrammar(language);
        if (grammar.language == nullptr) {
            continue;
        }

        // Shared with CodeHighlighter's preview rendering: the same language
        // means the same compiled query, cached once for the process rather
        // than per HighlightWorker instance.
        TSQuery* subQuery = cachedHighlightsQuery(grammar.language, grammar.highlights);
        if (subQuery == nullptr) {
            continue;
        }

        // A single level of injection: the sub-grammar's own injections
        // (HTML in prose, foreign code) are not recursed into yet.
        TreeSitterEngine sub;
        sub.setLanguage(grammar.language);
        sub.setText(std::string(source.substr(contentStart, contentEnd - contentStart)));
        if (sub.hasTree()) {
            // sub.source() — not the outer `source` — matches the coordinate
            // space of sub.rootNode()'s byte offsets, which predicate
            // evaluation needs to resolve capture text correctly.
            paintCaptures(subQuery, sub.rootNode(), contentStart, rangeStart, rangeEnd,
                          sub.source(), byteStyle);
        }
    }
    ts_query_cursor_delete(cursor);
}

} // namespace hungryeditor
