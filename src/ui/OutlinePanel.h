#pragma once

#include <QVector>
#include <QWidget>

#include "markdown/Outline.h"

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace hungryeditor {

/// Side panel listing the current document's headings as a tree. Activating an
/// entry asks the window to move the caret to that heading's source line;
/// highlightLine() keeps the selection tracking the caret.
class OutlinePanel : public QWidget
{
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget* parent = nullptr);

    /// Rebuild the tree from `headings`, nesting each under the nearest previous
    /// heading of a lower level. The vertical scroll position is preserved.
    void setHeadings(const QVector<outline::Heading>& headings);

    /// Select the last heading at or before `caretLine`, without emitting
    /// headingActivated.
    void highlightLine(int caretLine);

signals:
    void headingActivated(int line);

private:
    void onItemActivated(QTreeWidgetItem* item, int column);

    QTreeWidget* tree_ = nullptr;
    QLabel* empty_ = nullptr;
    bool suppressActivation_ = false;
};

} // namespace hungryeditor
