// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/LoginDialog.hpp"

#include "Application.hpp"
#include "common/Common.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "singletons/Settings.hpp"
#include "util/Clipboard.hpp"
#include "util/Helpers.hpp"
#include "widgets/ChoppaTitlebar.hpp"

#include <QPainter>
#include <QTabBar>

#ifdef USEWINSDK
#    include <Windows.h>
#endif

#include <pajlada/settings/setting.hpp>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

namespace chatterino {

namespace {

bool logInWithCredentials(QWidget *parent, const QString &userID,
                          const QString &username, const QString &clientID,
                          const QString &oauthToken)
{
    QStringList errors;

    if (userID.isEmpty())
    {
        errors.append("Missing user ID");
    }
    if (username.isEmpty())
    {
        errors.append("Missing username");
    }
    if (clientID.isEmpty())
    {
        errors.append("Missing Client ID");
    }
    if (oauthToken.isEmpty())
    {
        errors.append("Missing OAuth Token");
    }

    if (errors.length() > 0)
    {
        QMessageBox messageBox(parent);
        messageBox.setWindowTitle("Invalid account credentials");
        messageBox.setIcon(QMessageBox::Critical);
        messageBox.setText(errors.join("<br>"));
        messageBox.exec();
        return false;
    }

    std::string basePath = "/accounts/uid" + userID.toStdString();
    pajlada::Settings::Setting<QString>::set(basePath + "/username", username);
    pajlada::Settings::Setting<QString>::set(basePath + "/userID", userID);
    pajlada::Settings::Setting<QString>::set(basePath + "/clientID", clientID);
    pajlada::Settings::Setting<QString>::set(basePath + "/oauthToken",
                                             oauthToken);

    getApp()->getAccounts()->twitch.reloadUsers();
    getApp()->getAccounts()->twitch.currentUsername = username;
    getSettings()->requestSave();
    return true;
}

}  // namespace

BasicLoginWidget::BasicLoginWidget()
{
    const QString logInLink = "https://chatterino.com/client_login";
    this->setLayout(&this->ui_.layout);

    auto *intro =
        new QLabel(tr("<h2>Connect Twitch</h2>Open Twitch to authorize your "
                      "account, then paste the login information here."));
    intro->setWordWrap(true);
    this->ui_.layout.addWidget(intro);
    this->ui_.layout.setAlignment(Qt::AlignTop);
    this->ui_.layout.setSpacing(16);

    this->ui_.loginButton.setText("Log in (Opens in browser)");
    this->ui_.pasteCodeButton.setText("Paste login info");
    this->ui_.unableToOpenBrowserHelper.setWindowTitle(
        "choppa.lol - Unable to open browser");
    this->ui_.unableToOpenBrowserHelper.setWordWrap(true);
    this->ui_.unableToOpenBrowserHelper.hide();
    this->ui_.unableToOpenBrowserHelper.setText(
        QString("An error occurred while attempting to open <a href=\"%1\">the "
                "log in link (%1)</a> - open it manually in your browser and "
                "proceed from there.")
            .arg(logInLink));
    this->ui_.unableToOpenBrowserHelper.setOpenExternalLinks(true);

    this->ui_.horizontalLayout.addWidget(&this->ui_.loginButton);
    this->ui_.horizontalLayout.addWidget(&this->ui_.pasteCodeButton);

    this->ui_.layout.addLayout(&this->ui_.horizontalLayout);
    this->ui_.layout.addWidget(&this->ui_.unableToOpenBrowserHelper);

    connect(&this->ui_.loginButton, &QPushButton::clicked, [this, logInLink]() {
        qCDebug(chatterinoWidget) << "open login in browser";
        if (!QDesktopServices::openUrl(QUrl(logInLink)))
        {
            qCWarning(chatterinoWidget) << "open login in browser failed";
            this->ui_.unableToOpenBrowserHelper.show();
        }
    });

    connect(&this->ui_.pasteCodeButton, &QPushButton::clicked, [this]() {
        QStringList parameters = getClipboardText().split(";");
        QString oauthToken, clientID, username, userID;

        // Removing clipboard content to prevent accidental paste of credentials into somewhere
        crossPlatformCopy("");

        for (const auto &param : parameters)
        {
            QStringList kvParameters = param.split('=');
            if (kvParameters.size() != 2)
            {
                continue;
            }
            QString key = kvParameters[0];
            QString value = kvParameters[1];

            if (key == "oauth_token")
            {
                oauthToken = value;
            }
            else if (key == "client_id")
            {
                clientID = value;
            }
            else if (key == "username")
            {
                username = value;
            }
            else if (key == "user_id")
            {
                userID = value;
            }
            else
            {
                qCWarning(chatterinoWidget) << "Unknown key in code: " << key;
            }
        }

        if (logInWithCredentials(this, userID, username, clientID, oauthToken))
        {
            this->window()->close();
        }
    });
}

AdvancedLoginWidget::AdvancedLoginWidget()
{
    this->setLayout(&this->ui_.layout);

    this->ui_.instructionsLabel.setText("1. Fill in your username"
                                        "\n2. Fill in your user ID"
                                        "\n3. Fill in your client ID"
                                        "\n4. Fill in your OAuth token"
                                        "\n5. Press Add user");
    this->ui_.instructionsLabel.setWordWrap(true);

    this->ui_.layout.addWidget(&this->ui_.instructionsLabel);
    this->ui_.layout.addLayout(&this->ui_.formLayout);
    this->ui_.layout.addLayout(&this->ui_.buttonUpperRow.layout);

    this->refreshButtons();

    /// Form
    this->ui_.formLayout.addRow("Username", &this->ui_.usernameInput);
    this->ui_.formLayout.addRow("User ID", &this->ui_.userIDInput);
    this->ui_.formLayout.addRow("Client ID", &this->ui_.clientIDInput);
    this->ui_.formLayout.addRow("OAuth token", &this->ui_.oauthTokenInput);

    this->ui_.oauthTokenInput.setEchoMode(QLineEdit::Password);

    connect(&this->ui_.userIDInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });
    connect(&this->ui_.usernameInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });
    connect(&this->ui_.clientIDInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });
    connect(&this->ui_.oauthTokenInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });

    /// Upper button row

    this->ui_.buttonUpperRow.addUserButton.setText("Add user");
    this->ui_.buttonUpperRow.clearFieldsButton.setText("Clear fields");

    this->ui_.buttonUpperRow.layout.addWidget(
        &this->ui_.buttonUpperRow.addUserButton);
    this->ui_.buttonUpperRow.layout.addWidget(
        &this->ui_.buttonUpperRow.clearFieldsButton);

    connect(&this->ui_.buttonUpperRow.clearFieldsButton, &QPushButton::clicked,
            [this]() {
                this->ui_.userIDInput.clear();
                this->ui_.usernameInput.clear();
                this->ui_.clientIDInput.clear();
                this->ui_.oauthTokenInput.clear();
            });

    connect(&this->ui_.buttonUpperRow.addUserButton, &QPushButton::clicked,
            [this]() {
                QString userID = this->ui_.userIDInput.text();
                QString username = this->ui_.usernameInput.text();
                QString clientID = this->ui_.clientIDInput.text();
                QString oauthToken = this->ui_.oauthTokenInput.text();

                logInWithCredentials(this, userID, username, clientID,
                                     oauthToken);
            });
}

void AdvancedLoginWidget::refreshButtons()
{
    if (this->ui_.userIDInput.text().isEmpty() ||
        this->ui_.usernameInput.text().isEmpty() ||
        this->ui_.clientIDInput.text().isEmpty() ||
        this->ui_.oauthTokenInput.text().isEmpty())
    {
        this->ui_.buttonUpperRow.addUserButton.setEnabled(false);
    }
    else
    {
        this->ui_.buttonUpperRow.addUserButton.setEnabled(true);
    }
}

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    this->setFixedWidth(480);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowFlags(
        (this->windowFlags() & ~(Qt::WindowContextHelpButtonHint)) |
        Qt::Dialog | Qt::FramelessWindowHint);

    this->setWindowTitle("Add new account");

    this->setLayout(&this->ui_.mainLayout);
    this->ui_.mainLayout.setContentsMargins(1, 1, 1, 16);
    this->ui_.mainLayout.setSpacing(12);
    this->ui_.mainLayout.addWidget(new ChoppaTitlebar(this));
    auto *categories = new QHBoxLayout;
    categories->setContentsMargins(16, 0, 16, 0);
    auto *group = new QButtonGroup(this);
    const QStringList names{tr("Twitch"), tr("Manual setup"), tr("Kick")};
    for (int i = 0; i < names.size(); ++i)
    {
        auto *button = new QPushButton(names[i]);
        button->setCheckable(true);
        group->addButton(button, i);
        categories->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] {
            this->ui_.tabWidget.setCurrentIndex(i);
        });
        button->setChecked(i == 0);
    }
    this->ui_.mainLayout.addLayout(categories);
    this->ui_.mainLayout.addWidget(&this->ui_.tabWidget);

    this->ui_.tabWidget.addTab(&this->ui_.basic, "Basic");
    this->ui_.tabWidget.addTab(&this->ui_.advanced, "Advanced");

    this->ui_.buttonBox.setStandardButtons(QDialogButtonBox::Close);

    QObject::connect(&this->ui_.buttonBox, &QDialogButtonBox::rejected,
                     [this]() {
                         this->close();
                     });

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(16, 0, 16, 0);
    footer->addWidget(&this->ui_.buttonBox);
    this->ui_.mainLayout.addLayout(footer);

    this->ui_.tabWidget.addTab(&this->ui_.kick, "Kick");
    this->ui_.tabWidget.tabBar()->hide();
    this->ui_.kick.setProperty("choppaLoginSurface", true);
    for (int index = 0; index < this->ui_.tabWidget.count(); ++index)
    {
        auto *page = this->ui_.tabWidget.widget(index);
        page->layout()->setContentsMargins(16, 8, 16, 8);
        page->layout()->setSpacing(12);
    }
    auto fitPage = [this](int index) {
        auto *page = this->ui_.tabWidget.widget(index);
        const int width = this->width() - 2;
        const int preferred = page->layout()->hasHeightForWidth()
                                  ? page->layout()->totalHeightForWidth(width)
                                  : page->sizeHint().height();
        const int height =
            std::max(preferred, page->minimumSizeHint().height()) + 8;
        this->ui_.tabWidget.setFixedHeight(height);
        this->adjustSize();
    };
    // Resize only on navigation, not when member pages are being destroyed.
    connect(group, &QButtonGroup::idClicked, this, fitPage);
    this->setStyleSheet(R"(
        QDialog { color: white; }
        QLabel { color: #cccccc; font: 700 12px 'Satoshi'; background: transparent; border: none; }
        QTabWidget::pane { border: none; background: #111111; }
        QTabWidget > QWidget { border: none; }
        QPushButton { border: 1px solid #303030; border-radius: 6px; background: #1c1c1c; color: #cccccc; padding: 8px 12px; text-align: center; font: 700 12px 'Satoshi'; }
        QPushButton:hover, QPushButton:focus { background: #303030; color: white; }
        QPushButton:checked { background: #dddddd; color: #111111; }
        QLineEdit { border: 1px solid #303030; border-radius: 6px; background: #1a1a1a; color: white; padding: 8px; }
        QLineEdit:focus { border-color: #777777; }
    )");
    this->ensurePolished();
    fitPage(0);
}

void LoginDialog::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#383838"));
    painter.setBrush(QColor("#111111"));
    painter.drawRoundedRect(QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            7, 7);
}

}  // namespace chatterino
