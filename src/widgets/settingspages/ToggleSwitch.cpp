// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/ToggleSwitch.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>

namespace chatterino {

namespace {

    constexpr int TRACK_W = 40;
    constexpr int TRACK_H = 20;
    constexpr int WIDGET_W = 44;
    constexpr int LABEL_GAP = 8;

    QColor lerp(const QColor &a, const QColor &b, float t)
    {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                                a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF() + (b.blueF() - a.blueF()) * t);
    }

}  // namespace

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QCheckBox(parent)
    , animation_(new QPropertyAnimation(this, "knobPos", this))
{
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_Hover);

    this->knobPos_ = this->isChecked() ? 1.0F : 0.0F;
    this->animation_->setDuration(140);
    this->animation_->setEasingCurve(QEasingCurve::InOutCubic);

    QObject::connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        float target = on ? 1.0F : 0.0F;
        if (this->isVisible())
        {
            this->animation_->stop();
            this->animation_->setStartValue(this->knobPos_);
            this->animation_->setEndValue(target);
            this->animation_->start();
        }
        else
        {
            this->setKnobPos(target);
        }
    });
}

QSize ToggleSwitch::sizeHint() const
{
    if (this->text().isEmpty())
    {
        return {WIDGET_W, 24};
    }
    int textW = QFontMetrics(this->font()).horizontalAdvance(this->text());
    return {WIDGET_W + LABEL_GAP + textW, 24};
}

float ToggleSwitch::knobPos() const
{
    return this->knobPos_;
}

void ToggleSwitch::setKnobPos(float pos)
{
    this->knobPos_ = pos;
    this->update();
}

bool ToggleSwitch::hitButton(const QPoint &pos) const
{
    return this->rect().contains(pos);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal x = 2;
    const qreal y = (this->height() - TRACK_H) / 2.0;
    const QRectF track(x, y, TRACK_W, TRACK_H);
    const float t = this->knobPos_;

    painter.setPen(QPen(lerp(QColor("#444444"), QColor("#d6d6d6"), t), 1));
    painter.setBrush(lerp(QColor("#333333"), QColor("#d6d6d6"), t));
    painter.drawRoundedRect(track, TRACK_H / 2.0, TRACK_H / 2.0);

    const qreal knob = TRACK_H - 6;
    const qreal ky = y + 3;
    const qreal kxOff = x + 3;
    const qreal kxOn = x + TRACK_W - knob - 3;
    const qreal kx = kxOff + (kxOn - kxOff) * t;
    painter.setPen(Qt::NoPen);
    painter.setBrush(lerp(QColor("#999999"), QColor("#0a0a0a"), t));
    painter.drawEllipse(QRectF(kx, ky, knob, knob));

    if (!this->text().isEmpty())
    {
        painter.setPen(this->isEnabled() ? QColor("#ffffff")
                                         : QColor("#999999"));
        painter.drawText(
            QRectF(WIDGET_W + LABEL_GAP, 0, this->width() - WIDGET_W - LABEL_GAP,
                   this->height()),
            Qt::AlignVCenter | Qt::AlignLeft, this->text());
    }

    if (!this->isEnabled())
    {
        painter.fillRect(track, QColor(23, 23, 25, 140));
    }

    if (this->greyedOut)
    {
        painter.fillRect(this->rect(), QColor(20, 20, 22, 180));
    }
}

}  // namespace chatterino
