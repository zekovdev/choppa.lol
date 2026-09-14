// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/ChatterListWidget.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"  // IWYU pragma: keep
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Theme.hpp"
#include "util/Helpers.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QVBoxLayout>

namespace chatterino {

namespace {

QString formatVIPListError(HelixListVIPsError error, const QString &message)
{
    using Error = HelixListVIPsError;

    QString errorMessage = QString("Failed to list VIPs - ");

    switch (error)
    {
        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::Ratelimited: {
            errorMessage += "You are being ratelimited by Twitch. Try "
                            "again in a few seconds.";
        }
        break;

        case Error::UserMissingScope: {
            // TODO(pajlada): Phrase MISSING_REQUIRED_SCOPE
            errorMessage += "Missing required scope. "
                            "Re-login with your "
                            "account and try again.";
        }
        break;

        case Error::UserNotAuthorized: {
            // TODO(pajlada): Phrase MISSING_PERMISSION
            errorMessage += "You don't have permission to "
                            "perform that action.";
        }
        break;

        case Error::UserNotBroadcaster: {
            errorMessage +=
                "Due to Twitch restrictions, "
                "this command can only be used by the broadcaster. "
                "To see the list of VIPs you must use the Twitch website.";
        }
        break;

        case Error::Unknown: {
            errorMessage += "An unknown error has occurred.";
        }
        break;
    }
    return errorMessage;
}

QString formatModsError(HelixGetModeratorsError error, const QString &message)
{
    using Error = HelixGetModeratorsError;

    QString errorMessage = QString("Failed to get moderators: ");

    switch (error)
    {
        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::UserMissingScope: {
            errorMessage += "Missing required scope. "
                            "Re-login with your "
                            "account and try again.";
        }
        break;

        case Error::UserNotAuthorized: {
            errorMessage +=
                "Due to Twitch restrictions, "
                "this command can only be used by the broadcaster. "
                "To see the list of mods you must use the Twitch website.";
        }
        break;

        case Error::Unknown: {
            errorMessage += "An unknown error has occurred.";
        }
        break;
    }
    return errorMessage;
}

QString formatChattersError(HelixGetChattersError error, const QString &message)
{
    using Error = HelixGetChattersError;

    QString errorMessage = QString("Failed to get chatters: ");

    switch (error)
    {
        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::UserMissingScope: {
            errorMessage += "Missing required scope. "
                            "Re-login with your "
                            "account and try again.";
        }
        break;

        case Error::UserNotAuthorized: {
            errorMessage +=
                "Due to Twitch restrictions, "
                "this command can only be used by moderators. "
                "To see the list of chatters you must use the Twitch website.";
        }
        break;

        case Error::Unknown: {
            errorMessage += "An unknown error has occurred.";
        }
        break;
    }
    return errorMessage;
}

}  // namespace

ChatterListWidget::ChatterListWidget(const TwitchChannel *twitchChannel,
                                     QWidget *parent)
    : BaseWindow({BaseWindow::EnableCustomFrame, BaseWindow::ContentChrome},
                 parent)
{
    assert(twitchChannel != nullptr);
    this->setWindowTitle("Chatters · " + twitchChannel->getName());
    const QPointer<ChatterListWidget> alive(this);

    this->setAttribute(Qt::WA_DeleteOnClose);

    auto *dockVbox = new QVBoxLayout(this->getLayoutContainer());
    dockVbox->setContentsMargins(12, 12, 12, 12);
    dockVbox->setSpacing(10);
    auto *searchBar = new QLineEdit(this);

    auto *chattersList = new QListWidget();
    auto *resultList = new QListWidget();

    auto *loadingLabel = new QLabel("Loading...");
    searchBar->setPlaceholderText("Search User...");
    dockVbox->addWidget(searchBar);
    dockVbox->addWidget(loadingLabel);
    dockVbox->addWidget(chattersList, 1);
    dockVbox->addWidget(resultList, 1);
    resultList->hide();
    chattersList->setWordWrap(true);
    resultList->setWordWrap(true);

    auto formatListItemText = [](const QString &text) {
        auto *item = new QListWidgetItem();
        item->setText(text);
        item->setFont(
            getApp()->getFonts()->getFont(FontStyle::ChatMedium, 1.0));
        return item;
    };

    auto addLabel = [this, formatListItemText,
                     chattersList](const QString &label) {
        auto *formattedLabel = formatListItemText(label);
        formattedLabel->setFlags(Qt::NoItemFlags);
        formattedLabel->setForeground(this->theme->accent);
        chattersList->addItem(formattedLabel);
    };

    auto addUserList = [=](const QStringList &users, QString label) {
        if (users.isEmpty())
        {
            return;
        }

        addLabel(QString("%1 (%2)").arg(label, localizeNumbers(users.size())));

        for (const auto &user : users)
        {
            chattersList->addItem(formatListItemText(user));
        }
        chattersList->addItem(new QListWidgetItem());
    };

    auto performListSearch = [=]() {
        auto query = searchBar->text();
        if (query.isEmpty())
        {
            resultList->hide();
            chattersList->show();
            return;
        }

        auto results = chattersList->findItems(query, Qt::MatchContains);
        chattersList->hide();
        resultList->clear();
        for (auto &item : results)
        {
            if (item->flags().testFlag(Qt::ItemIsEnabled) &&
                !item->text().isEmpty() && !item->text().contains(' '))
            {
                resultList->addItem(formatListItemText(item->text()));
            }
        }
        resultList->show();
    };

    auto loadChatters = [=](auto modList, auto vipList, bool isBroadcaster) {
        getHelix()->getChatters(
            twitchChannel->roomId(),
            getApp()->getAccounts()->twitch.getCurrent()->getUserId(), 50000,
            [=](const auto &chatters) {
                if (!alive)
                    return;
                chattersList->setUpdatesEnabled(false);
                auto broadcaster = twitchChannel->getName().toLower();
                QStringList chatterList;
                QStringList modChatters;
                QStringList vipChatters;

                bool addedBroadcaster = false;
                for (auto chatter : chatters.chatters)
                {
                    chatter = chatter.toLower();

                    if (!addedBroadcaster && chatter == broadcaster)
                    {
                        addedBroadcaster = true;
                        addLabel("Broadcaster");
                        chattersList->addItem(broadcaster);
                        chattersList->addItem(new QListWidgetItem());
                        continue;
                    }

                    if (modList.contains(chatter))
                    {
                        modChatters.append(chatter);
                        continue;
                    }

                    if (vipList.contains(chatter))
                    {
                        vipChatters.append(chatter);
                        continue;
                    }

                    chatterList.append(chatter);
                }

                modChatters.sort();
                vipChatters.sort();
                chatterList.sort();

                if (isBroadcaster)
                {
                    addUserList(modChatters, QString("Moderators"));
                    addUserList(vipChatters, QString("VIPs"));
                }
                else
                {
                    addLabel("Moderators");
                    chattersList->addItem(
                        "Moderators cannot check who is a moderator");
                    chattersList->addItem(new QListWidgetItem());

                    addLabel("VIPs");
                    chattersList->addItem(
                        "Moderators cannot check who is a VIP");
                    chattersList->addItem(new QListWidgetItem());
                }

                addUserList(chatterList, QString("Chatters"));

                loadingLabel->hide();
                chattersList->setUpdatesEnabled(true);
                performListSearch();
            },
            [alive, loadingLabel, chattersList, formatListItemText](
                auto error, const auto &message) {
                if (!alive)
                    return;
                loadingLabel->hide();
                auto errorMessage = formatChattersError(error, message);
                chattersList->addItem(formatListItemText(errorMessage));
            });
    };

    QObject::connect(searchBar, &QLineEdit::textEdited, this,
                     performListSearch);

    // Only broadcaster can get vips, mods can get chatters
    if (twitchChannel->isBroadcaster())
    {
        // Add moderators
        getHelix()->getModerators(
            twitchChannel->roomId(), 1000,
            [=](const auto &mods) {
                if (!alive)
                    return;
                QSet<QString> modList;
                for (const auto &mod : mods)
                {
                    modList.insert(mod.userName.toLower());
                }

                // Add vips
                getHelix()->getChannelVIPs(
                    twitchChannel->roomId(),
                    [=](const auto &vips) {
                        if (!alive)
                            return;
                        QSet<QString> vipList;
                        for (const auto &vip : vips)
                        {
                            vipList.insert(vip.userName.toLower());
                        }

                        // Add chatters
                        loadChatters(modList, vipList, true);
                    },
                    [alive, loadingLabel, chattersList, formatListItemText](
                        auto error, const auto &message) {
                        if (!alive)
                            return;
                        loadingLabel->hide();
                        auto errorMessage = formatVIPListError(error, message);
                        chattersList->addItem(formatListItemText(errorMessage));
                    });
            },
            [alive, loadingLabel, chattersList, formatListItemText](
                auto error, const auto &message) {
                if (!alive)
                    return;
                loadingLabel->hide();
                auto errorMessage = formatModsError(error, message);
                chattersList->addItem(formatListItemText(errorMessage));
            });
    }
    else if (twitchChannel->hasModRights())
    {
        QSet<QString> modList;
        QSet<QString> vipList;
        loadChatters(modList, vipList, false);
    }
    else
    {
        chattersList->addItem(
            formatListItemText("Due to Twitch restrictions, this feature is "
                               "only \navailable for moderators."));
        chattersList->addItem(
            formatListItemText("If you would like to see the Chatter list, you "
                               "must \nuse the Twitch website."));
        loadingLabel->hide();
    }

    this->setMinimumSize(320, 280);
    this->resize(380, 440);

    auto listDoubleClick = [this](const QModelIndex &index) {
        const auto itemText = index.data().toString();

        if (!itemText.isEmpty() && !itemText.contains(' ') &&
            index.flags().testFlag(Qt::ItemIsEnabled))
        {
            this->userClicked(itemText);
        }
    };

    QObject::connect(chattersList, &QListWidget::doubleClicked, this,
                     listDoubleClick);

    QObject::connect(resultList, &QListWidget::doubleClicked, this,
                     listDoubleClick);

    HotkeyController::HotkeyMap actions{
        {"delete",
         [this](const std::vector<QString> &) -> QString {
             this->close();
             return "";
         }},
        {"accept", nullptr},
        {"reject", nullptr},
        {"scrollPage", nullptr},
        {"openTab", nullptr},
        {"search",
         [searchBar](const std::vector<QString> &) -> QString {
             searchBar->setFocus();
             searchBar->selectAll();
             return "";
         }},
    };

    getApp()->getHotkeys()->shortcutsForCategory(HotkeyCategory::PopupWindow,
                                                 actions, this);


    this->setStyleSheet(R"(
        QLabel { color: #999999; background: transparent; }
        QLineEdit { background: #1a1a1a; color: #eeeeee; border: 1px solid #383838; border-radius: 6px; padding: 8px; }
        QLineEdit:focus { border-color: #777777; }
        QListWidget { background: #111111; color: #dddddd; border: none; outline: none; font: 12px 'Outfit'; }
        QListWidget::item { padding: 4px 8px; border-radius: 4px; }
        QListWidget::item:selected { background: #303030; color: white; }
        QListWidget::item:hover { background: #202020; }
        QScrollBar:vertical { width: 6px; background: #111111; }
        QScrollBar::handle:vertical { background: #383838; min-height: 24px; border-radius: 3px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    )");
}

}  // namespace chatterino
