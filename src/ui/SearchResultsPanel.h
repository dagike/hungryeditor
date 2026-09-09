#pragma once

#include <QList>
#include <QWidget>

#include "workspace/FileSearch.h"

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace hungryeditor {

/// Bottom panel that lists find-in-files hits grouped by file. Activating a
/// line row asks the window to open that file at that line.
class SearchResultsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SearchResultsPanel(QWidget* parent = nullptr);

    void showResults(const QString& query, const QList<FileSearchHit>& hits);
    void clearResults();

    const QList<FileSearchHit>& hits() const { return hits_; }
    /// Re-emit resultActivated for the hit at `index` — the click path, and a
    /// seam for keyboard result navigation.
    void activateResult(int index);

signals:
    void resultActivated(const QString& path, int line);

private:
    void onItemActivated(QTreeWidgetItem* item, int column);

    QLabel* summary_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QList<FileSearchHit> hits_;
};

} // namespace hungryeditor
