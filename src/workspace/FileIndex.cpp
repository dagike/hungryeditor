#include "workspace/FileIndex.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QtConcurrent>

namespace hungryeditor {

namespace {

const QStringList kIgnoredDirs{QStringLiteral(".git"),   QStringLiteral(".hg"),
                               QStringLiteral(".svn"),   QStringLiteral("node_modules"),
                               QStringLiteral(".cache"), QStringLiteral("build")};

constexpr qint64 kMaxFileBytes = 8LL * 1024 * 1024;
constexpr int kMaxFiles = 20000;

QStringList scan(const QString& directory)
{
    QStringList paths;
    QDirIterator it(directory, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext() && paths.size() < kMaxFiles) {
        const QString path = it.next();
        const QString relative = QDir(directory).relativeFilePath(path);
        bool ignored = false;
        for (const QString& segment : relative.split(QLatin1Char('/'))) {
            if (segment.startsWith(QLatin1Char('.')) || kIgnoredDirs.contains(segment)) {
                ignored = true;
                break;
            }
        }
        if (ignored) {
            continue;
        }
        if (QFileInfo(path).size() > kMaxFileBytes) {
            continue;
        }
        paths.append(path);
    }
    return paths;
}

} // namespace

FileIndex::FileIndex(QObject* parent) : QObject(parent)
{
    connect(&watcher_, &QFutureWatcher<QStringList>::finished, this, [this] {
        files_ = watcher_.result();
        emit refreshed();
    });
}

void FileIndex::setRoot(const QString& directory)
{
    if (directory == root_ && watcher_.isRunning()) {
        return;
    }
    root_ = directory;
    watcher_.setFuture(QtConcurrent::run(scan, directory));
}

void FileIndex::refresh()
{
    if (!root_.isEmpty()) {
        watcher_.setFuture(QtConcurrent::run(scan, root_));
    }
}

} // namespace hungryeditor
