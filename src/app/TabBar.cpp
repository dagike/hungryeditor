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
    // Not in the Tab order: switching/closing tabs already has full keyboard
    // coverage (Ctrl+PageUp/Down, Ctrl+Tab, Ctrl+W, Ctrl+P), so this is a
    // deliberate choice, not a gap — a screen reader can still reach it via
    // other navigation, hence the accessible name below.
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName(tr("Open documents"));
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
