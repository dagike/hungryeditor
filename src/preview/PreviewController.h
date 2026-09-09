#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "markdown/Md4cRenderer.h"

namespace hungryeditor {

class PreviewBackend;

/// Sits between the editor and a PreviewBackend: coalesces rapid edits, renders
/// the Markdown to HTML once typing pauses, and streams the result to the
/// preview. Keeping the debounce here means a keystroke does not re-run the
/// parser.
class PreviewController : public QObject
{
    Q_OBJECT

public:
    explicit PreviewController(PreviewBackend* backend, QObject* parent = nullptr);

    /// Milliseconds of quiet before a pending edit is rendered (default 50).
    void setDebounceInterval(int milliseconds);
    int debounceInterval() const;

public slots:
    /// The editor's current text. Restarts the debounce; the render follows.
    void setMarkdown(const QString& markdown);

    /// Render whatever is pending right now, skipping the debounce — used on a
    /// document switch so the preview never lags a tab change.
    void flush();

signals:
    /// The HTML fragment produced by each render pushed to the preview.
    void rendered(const QString& html);

private:
    void render();

    PreviewBackend* backend_;
    Md4cRenderer renderer_;
    QTimer timer_;
    QString pending_;
    bool dirty_ = false;
};

} // namespace hungryeditor
