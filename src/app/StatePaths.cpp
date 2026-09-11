#include "app/StatePaths.h"

#include <QDir>
#include <QStandardPaths>

namespace hungryeditor {

QString defaultStateDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath() + QLatin1String("/hungryeditor");
    }
    return base;
}

} // namespace hungryeditor
