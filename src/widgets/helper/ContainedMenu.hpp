#pragma once

#include <QEvent>
#include <QMenu>
#include <QPointer>

namespace chatterino {

class ContainedMenu final : public QObject
{
public:
    ContainedMenu(QMenu *menu, QWidget *host)
        : QObject(menu), menu_(menu), host_(host)
    {
        menu->installEventFilter(this);
        menu->setMaximumHeight(qMax(80, host->height() - 16));
        menu->setMaximumWidth(qMax(80, host->width() - 16));
    }

    static void install(QMenu *menu, QWidget *host)
    {
        new ContainedMenu(menu, host);
        for (auto *action : menu->actions())
            if (auto *child = action->menu())
                install(child, host);
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Show && host_ && menu_)
        {
            const QRect area(host_->mapToGlobal(QPoint(8, 8)),
                             host_->size() - QSize(16, 16));
            menu_->move(qBound(area.left(), menu_->x(),
                               qMax(area.left(), area.right() - menu_->width() + 1)),
                        qBound(area.top(), menu_->y(),
                               qMax(area.top(), area.bottom() - menu_->height() + 1)));
        }
        return false;
    }

private:
    QPointer<QMenu> menu_;
    QPointer<QWidget> host_;
};

}
