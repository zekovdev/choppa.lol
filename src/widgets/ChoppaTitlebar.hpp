// SPDX-License-Identifier: MIT
#pragma once

#include <QWidget>

namespace chatterino {
// Shared chrome for the desktop and settings. Native hit testing in BaseWindow
// retains resizing and caption dragging; Qt handles the accessible controls.
class ChoppaTitlebar : public QWidget
{
public:
    explicit ChoppaTitlebar(QWidget *window);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};
}  // namespace chatterino
