#include "highlight/HighlightWorker.h"

#include <string>
#include <string_view>
#include <vector>

#include <QByteArray>
#include <QTimer>

#include "highlight/CaptureStyles.h"

namespace hungryeditor {

namespace {
constexpr int kDebounceMs = 15;
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
    if (query_ != nullptr) {
        ts_query_delete(query_);
    }
}

int HighlightWorker::debounceIntervalMs()
{
    return kDebounceMs;
}

void HighlightWorker::configure(const TSLanguage* language, const QString& highlightQuery)
{
    engine_.setLanguage(language);

    if (query_ != nullptr) {
        ts_query_delete(query_);
        query_ = nullptr;
    }
    if (language == nullptr || highlightQuery.isEmpty()) {
        return;
    }

    const QByteArray utf8 = highlightQuery.toUtf8();
    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    query_ = ts_query_new(language, utf8.constData(), static_cast<uint32_t>(utf8.size()),
                          &errorOffset, &errorType);
    // A malformed query simply disables highlighting; it is not fatal.
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
    if (query_ == nullptr || !engine_.hasTree() || source.empty()) {
        return spans;
    }

    // Paint a per-byte style buffer, then run-length encode it. Later captures
    // (which tree-sitter yields in node order, more specific ones last) win.
    std::vector<qint32> byteStyle(source.size(), StylePlain);

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query_, engine_.rootNode());

    TSQueryMatch match;
    uint32_t captureIndex = 0;
    while (ts_query_cursor_next_capture(cursor, &match, &captureIndex)) {
        const TSQueryCapture& capture = match.captures[captureIndex];

        uint32_t nameLen = 0;
        const char* name = ts_query_capture_name_for_id(query_, capture.index, &nameLen);
        const int style = styleForCapture(std::string_view(name, nameLen));
        if (style == StylePlain) {
            continue;
        }

        const uint32_t start = ts_node_start_byte(capture.node);
        const uint32_t end = ts_node_end_byte(capture.node);
        for (uint32_t i = start; i < end && i < byteStyle.size(); ++i) {
            byteStyle[i] = style;
        }
    }
    ts_query_cursor_delete(cursor);

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

} // namespace hungryeditor
