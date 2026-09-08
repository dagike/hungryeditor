#include "highlight/HighlightController.h"

#include <QMetaObject>
#include <QThread>

namespace hungryeditor {

HighlightController::HighlightController(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<HighlightResult>();
    qRegisterMetaType<const TSLanguage*>("const TSLanguage*");

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

void HighlightController::configure(const TSLanguage* language, const QString& highlightQuery)
{
    QMetaObject::invokeMethod(worker_, "configure", Qt::QueuedConnection,
                              Q_ARG(const TSLanguage*, language), Q_ARG(QString, highlightQuery));
}

quint64 HighlightController::submit(const QString& text)
{
    const quint64 revision = ++revision_;
    QMetaObject::invokeMethod(worker_, "submit", Qt::QueuedConnection, Q_ARG(QString, text),
                              Q_ARG(quint64, revision));
    return revision;
}

void HighlightController::onParsed(const HighlightResult& result)
{
    // Drop results that a newer submission has already superseded.
    if (result.revision < lastResultRevision_) {
        return;
    }
    lastResultRevision_ = result.revision;
    emit highlighted(result);
}

} // namespace hungryeditor
