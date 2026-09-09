#include "app/TabBar.h"

#include <QMouseEvent>

namespace hungryeditor {

TabBar::TabBar(QWidget* parent) : QTabBar(parent)
{
    setDocumentMode(true);
    setMovable(true);
    setTabsClosable(true);
    setExpanding(false);
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideRight);
    setFocusPolicy(Qt::NoFocus);
}

void TabBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0) {
            emit tabCloseRequested(index);
            event->accept();
            return;
        }
    }
    QTabBar::mouseReleaseEvent(event);
}

} // namespace hungryeditor
