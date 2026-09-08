#include "highlight/HighlightWorker.h"

#include <string>

#include <QTimer>

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

HighlightWorker::~HighlightWorker() = default;

int HighlightWorker::debounceIntervalMs()
{
    return kDebounceMs;
}

void HighlightWorker::setLanguage(const TSLanguage* language)
{
    engine_.setLanguage(language);
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
    engine_.setText(pendingText_.toStdString());
    pendingText_.clear();

    HighlightResult result;
    result.revision = revision;
    if (engine_.hasTree()) {
        const TSNode root = engine_.rootNode();
        result.rootType = QString::fromUtf8(ts_node_type(root));
        result.namedChildCount = static_cast<int>(ts_node_named_child_count(root));
        result.ok = true;
    }
    emit parsed(result);
}

} // namespace hungryeditor
