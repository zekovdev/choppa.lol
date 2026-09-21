// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QCheckBox>

class QPropertyAnimation;

namespace chatterino {

/// A sliding on/off toggle switch in the style of the 7TV extension. It
/// behaves exactly like a QCheckBox (same checked/toggled API) but paints a
/// rounded track with a knob that slides and fades between states.
class ToggleSwitch : public QCheckBox
{
    Q_OBJECT
    Q_PROPERTY(float knobPos READ knobPos WRITE setKnobPos)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    float knobPos() const;
    void setKnobPos(float pos);

    bool greyedOut{};

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;

private:
    float knobPos_ = 0.0F;
    QPropertyAnimation *animation_;
};

}  // namespace chatterino
