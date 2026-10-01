// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/SettingsDialogTab.hpp"

#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/settingspages/SettingsPage.hpp"

#include <QPainter>
#include <QStyleOption>

namespace chatterino {

SettingsDialogTab::SettingsDialogTab(SettingsDialog *_dialog,
                                     std::function<SettingsPage *()> _lazyPage,
                                     const QString &name, QString imageFileName,
                                     SettingsTabId id)
    : BaseWidget(_dialog)
    , dialog_(_dialog)
    , lazyPage_(std::move(_lazyPage))
    , id_(id)
    , name_(name)
{
    this->ui_.labelText = name;
    this->ui_.icon.addFile(imageFileName);

    this->setCursor(QCursor(Qt::PointingHandCursor));
    this->setFocusPolicy(Qt::StrongFocus);
    this->setAccessibleName(name);
    this->setToolTip(name);

    this->setStyleSheet("color: #b0b0b6");
}

void SettingsDialogTab::setSelected(bool _selected)
{
    if (this->selected_ == _selected)
    {
        return;
    }

    //    height: <checkbox-size>px;

    this->selected_ = _selected;
    this->update();
    this->selectedChanged(this->selected_);
}

SettingsPage *SettingsDialogTab::page()
{
    if (this->page_)
    {
        return this->page_;
    }

    this->page_ = this->lazyPage_();
    this->page_->setTab(this);
    return this->page_;
}

void SettingsDialogTab::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QStyleOption opt;
    opt.initFrom(this);

    this->style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(
        QColor(this->selected_ || this->hasFocus() ? "#666666" : "#292929"));
    painter.setBrush(QColor(this->selected_ ? "#242424" : "#151515"));
    painter.drawRoundedRect(this->rect().adjusted(1, 1, -1, -1), 6, 6);
    painter.setPen(QColor(this->selected_ ? "#ffffff" : "#aaaaaa"));
    painter.drawText(
        this->rect().adjusted(5, 0, -5, 0), Qt::AlignCenter,
        this->fontMetrics().elidedText(this->ui_.labelText, Qt::ElideRight,
                                       this->width() - 10));
}

void SettingsDialogTab::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    this->dialog_->selectTab(this);

    this->setFocus();
}

const QString &SettingsDialogTab::name() const
{
    return this->name_;
}

SettingsTabId SettingsDialogTab::id() const
{
    return this->id_;
}

}  // namespace chatterino
