#pragma once

#include "widgets/helper/PopupGeometry.hpp"

#include <QEvent>
#include <QMenu>
#include <QPointer>

namespace chatterino {

class ContainedMenu final : public QObject
{
public:
    ContainedMenu(QMenu *menu, QWidget *host)
        : QObject(menu)
        , menu_(menu)
        , host_(host)
    {
        menu->installEventFilter(this);
        host->installEventFilter(this);
        menu->setMaximumSize(popupAvailableRect(host).size());
    }

    static void install(QMenu *menu, QWidget *host)
    {
        if (!menu->property("choppaContained").toBool())
        {
            menu->setProperty("choppaContained", true);
            new ContainedMenu(menu, host);
        }
        for (auto *action : menu->actions())
            if (auto *child = action->menu())
                install(child, host);
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (host_ && menu_ &&
            (event->type() == QEvent::Show ||
             (object == host_ && menu_->isVisible() &&
              (event->type() == QEvent::Resize ||
               event->type() == QEvent::Move))))
        {
            const auto area = popupAvailableRect(host_);
            menu_->setMaximumSize(area.size());
            menu_->resize(menu_->size().boundedTo(area.size()));
            menu_->move(
                containPopupPosition(area, menu_->size(), menu_->pos()));
        }
        return false;
    }

private:
    QPointer<QMenu> menu_;
    QPointer<QWidget> host_;
};

}  // namespace chatterino
