#include "highlight/HighlightController.h"

#include <QMetaObject>
#include <QThread>

namespace hungryeditor {

HighlightController::HighlightController(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<HighlightResult>();
    qRegisterMetaType<const TSLanguage*>("const TSLanguage*");
    // Explicit name, matching the Q_ARG(PendingEdit, ...) spelling in
    // submit() below: qRegisterMetaType<T>() without one would register
    // under the type's fully-qualified name, but string-based invokeMethod
    // resolves Q_ARG's argument type by looking up exactly the name it was
    // given — see the const TSLanguage* line just above for the same fix.
    qRegisterMetaType<PendingEdit>("PendingEdit");
    qRegisterMetaType<HighlightRange>("HighlightRange");

    thread_ = new QThread(this);
    thread_->setObjectName(QStringLiteral("highlight-worker"));

    worker_ = new HighlightWorker;
    worker_->moveToThread(thread_);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &HighlightWorker::parsed, this, &HighlightController::onParsed);

    thread_->start();
}

HighlightController::~HighlightController()
{
    thread_->quit();
    thread_->wait();
}

void HighlightController::configure(const TSLanguage* language, const QString& highlightQuery,
                                    const QString& injectionQuery)
{
    QMetaObject::invokeMethod(worker_, "configure", Qt::QueuedConnection,
                              Q_ARG(const TSLanguage*, language), Q_ARG(QString, highlightQuery),
                              Q_ARG(QString, injectionQuery));
}

quint64 HighlightController::submit(const QString& text, PendingEdit edit, HighlightRange range,
                                    bool textChanged)
{
    if (!enabled_) {
        return revision_;
    }
    const quint64 revision = ++revision_;
    QMetaObject::invokeMethod(worker_, "submit", Qt::QueuedConnection, Q_ARG(QString, text),
                              Q_ARG(quint64, revision), Q_ARG(PendingEdit, edit),
                              Q_ARG(HighlightRange, range), Q_ARG(bool, textChanged));
    return revision;
}

void HighlightController::setEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    if (!enabled_) {
        // Ignore any parse still in flight for the text we are leaving behind.
        lastResultRevision_ = revision_ + 1;
    }
}

void HighlightController::onParsed(const HighlightResult& result)
{
    // Drop results that a newer submission has already superseded, or any
    // result at all while disabled.
    if (!enabled_ || result.revision < lastResultRevision_) {
        return;
    }
    lastResultRevision_ = result.revision;
    emit highlighted(result);
}

} // namespace hungryeditor
