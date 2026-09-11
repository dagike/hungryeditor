#include "highlight/HighlightWorker.h"

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

void HighlightWorker::submit(const QString& text, quint64 revision)
{
    pendingText_ = text;
    pendingRevision_ = revision;
    havePending_ = true;
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

    engine_.setText(source);

    HighlightResult result;
    result.revision = revision;
    if (engine_.hasTree()) {
        const TSNode root = engine_.rootNode();
        result.rootType = QString::fromUtf8(ts_node_type(root));
        result.namedChildCount = static_cast<int>(ts_node_named_child_count(root));
        result.ok = true;
        result.spans = computeSpans(source);
    }
    emit parsed(result);
}

QVector<HighlightSpan> HighlightWorker::computeSpans(std::string_view source) const
{
    QVector<HighlightSpan> spans;
    if (!engine_.hasTree() || source.empty()) {
        return spans;
    }

    // Paint a per-byte style buffer, then run-length encode it. Later captures
    // (which tree-sitter yields in node order, more specific ones last) win;
    // injected sub-grammars are painted last and override the block layer.
    std::vector<qint32> byteStyle(source.size(), StylePlain);
    if (query_ != nullptr) {
        paintCaptures(query_, engine_.rootNode(), 0, source, byteStyle);
    }
    paintInjections(source, byteStyle);

    for (uint32_t i = 0; i < byteStyle.size();) {
        const qint32 style = byteStyle[i];
        uint32_t j = i + 1;
        while (j < byteStyle.size() && byteStyle[j] == style) {
            ++j;
        }
        spans.push_back(HighlightSpan{i, j - i, style});
        i = j;
    }
    return spans;
}

void HighlightWorker::paintCaptures(TSQuery* query, const TSNode& root, quint32 baseOffset,
                                    std::string_view textForPredicates,
                                    std::vector<qint32>& byteStyle) const
{
    TSQueryCursor* cursor = ts_query_cursor_new();
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
            for (quint32 j = start; j < end && j < byteStyle.size(); ++j) {
                byteStyle[j] = style;
            }
        }
    }
    ts_query_cursor_delete(cursor);
}

void HighlightWorker::paintInjections(std::string_view source, std::vector<qint32>& byteStyle) const
{
    if (injectionQuery_ == nullptr) {
        return;
    }

    TSQueryCursor* cursor = ts_query_cursor_new();
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
        if (language.empty()) {
            language = predicates::directiveLanguage(injectionQuery_, match.pattern_index);
        }

        const Grammar grammar = injectedGrammar(language);
        if (grammar.language == nullptr) {
            continue;
        }

        const uint32_t contentStart = ts_node_start_byte(contentNode);
        const uint32_t contentEnd = ts_node_end_byte(contentNode);
        if (contentEnd > source.size() || contentEnd <= contentStart) {
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
            paintCaptures(subQuery, sub.rootNode(), contentStart, sub.source(), byteStyle);
        }
    }
    ts_query_cursor_delete(cursor);
}

} // namespace hungryeditor
