// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/AccountSwitchPopup.hpp"

#include "Application.hpp"
#include "common/Literals.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "widgets/AccountSwitchWidget.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/helper/KickAccountSwitchWidget.hpp"
#include "widgets/helper/MicroNotebook.hpp"

#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QPushButton>

namespace chatterino {

using namespace literals;

AccountSwitchPopup::AccountSwitchPopup(QWidget *parent)
    : BaseWindow(
          {
              BaseWindow::TopMost,
              BaseWindow::Frameless,
              BaseWindow::DisableLayoutSave,
              BaseWindow::LinuxPopup,
          },
          parent)
{
    this->focusOutAction = FocusOutAction::Hide;

    this->setContentsMargins(0, 0, 0, 0);

    auto *notebook = new MicroNotebook(this);

    this->ui_.accountSwitchWidget = new AccountSwitchWidget(this);
    this->ui_.accountSwitchWidget->setFocusPolicy(Qt::NoFocus);
    this->ui_.kickAccountSwitcher = new KickAccountSwitchWidget(this);
    this->ui_.kickAccountSwitcher->setFocusPolicy(Qt::NoFocus);

    auto updateNotebook = [this, notebook] {
        if (getApp()->getAccounts()->kick.accounts.empty())
        {
            notebook->setShowHeader(false);
            notebook->select(this->ui_.accountSwitchWidget);
        }
        else
        {
            notebook->setShowHeader(true);
        }
    };
    this->signalHolder_.addConnection(
        getApp()->getAccounts()->kick.userListUpdated.connect(updateNotebook));

    notebook->addPage(this->ui_.accountSwitchWidget, "Twitch");
    notebook->addPage(this->ui_.kickAccountSwitcher, "Kick");
    updateNotebook();
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);
    auto *heading = new QLabel(tr("Accounts"));
    heading->setObjectName("accountPopupHeading");
    heading->setStyleSheet(
        "color: #a0a0a0; font: 700 12px 'Satoshi'; padding: 2px 6px;");
    vbox->addWidget(heading);
    vbox->addWidget(notebook);

    auto *hbox = new QHBoxLayout();
    auto *manageAccountsButton = new QPushButton(this);
    manageAccountsButton->setObjectName("manageAccounts");
    manageAccountsButton->setText("Manage Accounts");
    manageAccountsButton->setFocusPolicy(Qt::NoFocus);
    hbox->addWidget(manageAccountsButton);
    vbox->addLayout(hbox);

    connect(manageAccountsButton, &QPushButton::clicked, [this]() {
        this->hide();
        SettingsDialog::showDialog(this->parentWidget(),
                                   SettingsDialogPreference::Accounts);
    });
    connect(this->ui_.accountSwitchWidget, &QListWidget::clicked, this,
            &QWidget::hide);
    connect(this->ui_.kickAccountSwitcher, &QListWidget::clicked, this,
            &QWidget::hide);

    this->getLayoutContainer()->setLayout(vbox);

    this->themeChangedEvent();
    this->refresh();
}

void AccountSwitchPopup::themeChangedEvent()
{
    BaseWindow::themeChangedEvent();

    this->setStyleSheet(uR"(
        QListView {
            border: none;
            outline: none;
            font-family: Satoshi;
            font-size: 12px;
            color: #eeeeee;
            background: #111111;
        }
        QListView::item {
            padding: 6px 8px;
            border-radius: 6px;
        }
        QListView::item:hover {
            background: #292929;
        }
        QListView::item:selected {
            background: #202020;
            color: #eeeeee;
        }

        QPushButton {
            border: 1px solid #383838;
            border-radius: 6px;
            min-height: 30px;
            padding: 0 8px;
            background: #1a1a1a;
            color: #eeeeee;
            font: 700 12px 'Satoshi';
        }
        QPushButton:hover {
            background: #292929;
        }
        QPushButton:pressed {
            background: #202020;
        }
        QPushButton:focus {
            border-color: #888888;
        }

        chatterino--AccountSwitchPopup {
            background: #111111;
        }
    )"_s);
}

void AccountSwitchPopup::refresh()
{
    this->ui_.accountSwitchWidget->refresh();
    this->ui_.kickAccountSwitcher->refresh();
    const bool hasKick = !getApp()->getAccounts()->kick.accounts.empty();
    const int rows =
        std::clamp(std::max(this->ui_.accountSwitchWidget->count(),
                            this->ui_.kickAccountSwitcher->count()),
                   1, 8);
    const auto scrollPolicy =
        std::max(this->ui_.accountSwitchWidget->count(),
                 this->ui_.kickAccountSwitcher->count()) > 8
            ? Qt::ScrollBarAsNeeded
            : Qt::ScrollBarAlwaysOff;
    this->ui_.accountSwitchWidget->setVerticalScrollBarPolicy(scrollPolicy);
    this->ui_.kickAccountSwitcher->setVerticalScrollBarPolicy(scrollPolicy);
    this->ui_.accountSwitchWidget->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    this->ui_.kickAccountSwitcher->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    this->setScaleIndependentSize(240, 112 + rows * 30 + (hasKick ? 36 : 0));
}

void AccountSwitchPopup::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor("#111111"));
    painter.setPen(QColor("#383838"));
    painter.drawRoundedRect(this->rect().adjusted(1, 1, -1, -1), 8, 8);
}

}  // namespace chatterino
