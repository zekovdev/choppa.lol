// SPDX-License-Identifier: MIT
#pragma once

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace chatterino {

// Both rectangles use logical, global Qt coordinates, including mixed-DPI desktops.
inline QRect popupAvailableRect(QWidget *host)
{
    host = host->window();
    const QRect bounds(host->mapToGlobal(QPoint()), host->size());
    auto *screen = QGuiApplication::screenAt(bounds.center());
    if (!screen)
        screen = host->screen();
    const auto workArea = screen->availableGeometry();
    const auto visible = bounds.intersected(workArea);
    return (visible.isEmpty() ? workArea : visible).adjusted(8, 8, -8, -8);
}

inline QPoint containPopupPosition(const QRect &area, QSize size,
                                   QPoint preferred)
{
    return {qBound(area.left(), preferred.x(),
                   qMax(area.left(), area.right() - size.width() + 1)),
            qBound(area.top(), preferred.y(),
                   qMax(area.top(), area.bottom() - size.height() + 1))};
}

}  // namespace chatterino
