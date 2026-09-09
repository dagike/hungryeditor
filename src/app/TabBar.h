#pragma once

#include <QTabBar>

namespace hungryeditor {

/// The document tab strip. A plain QTabBar in document mode: movable,
/// closable, non-expanding, with middle-click-to-close. It carries no state
/// of its own — MainWindow keeps it in step with the DocumentManager.
class TabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit TabBar(QWidget* parent = nullptr);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
};

} // namespace hungryeditor
