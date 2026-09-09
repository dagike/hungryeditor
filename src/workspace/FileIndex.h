#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

namespace hungryeditor {

/// A background-refreshed list of the files under a root directory, for the
/// quick-open picker. `.git`, `node_modules`, hidden directories and oversized
/// files are skipped. Scanning runs off the GUI thread.
class FileIndex : public QObject
{
    Q_OBJECT

public:
    explicit FileIndex(QObject* parent = nullptr);

    QString root() const { return root_; }

    /// Point the index at `directory` and kick off a background rescan. A no-op
    /// if the root is unchanged and a scan is not already stale.
    void setRoot(const QString& directory);

    /// The most recent snapshot of absolute file paths (empty until the first
    /// scan completes).
    QStringList files() const { return files_; }

signals:
    /// A scan finished and files() now reflects it.
    void refreshed();

private:
    QString root_;
    QStringList files_;
    QFutureWatcher<QStringList> watcher_;
};

} // namespace hungryeditor
