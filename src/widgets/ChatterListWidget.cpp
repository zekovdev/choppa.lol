// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
// SPDX-License-Identifier: MIT
#include "widgets/ChatterListWidget.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

namespace chatterino {
namespace {
constexpr int UserLoginRole = Qt::UserRole + 1;
}

ChatterListWidget::ChatterListWidget(const TwitchChannel *channel,
                                     QWidget *parent)
    : BaseWindow({BaseWindow::EnableCustomFrame, BaseWindow::ContentChrome},
                 parent)
    , channelID_(channel->roomId())
    , channelName_(channel->getName().toLower())
    , isBroadcaster_(channel->isBroadcaster())
    , canLoad_(channel->hasModRights() || channel->isBroadcaster())
{
    setWindowTitle("Chatters in #" + channelName_);
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(320, 280);
    resize(380, 440);
    auto *layout = new QVBoxLayout(getLayoutContainer());
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    search_ = new QLineEdit;
    search_->setPlaceholderText(tr("Search users…"));
    search_->setAccessibleName(tr("Search users"));
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);
    status_ = new QLabel;
    status_->setObjectName("chatterStatus");
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    model_ = new QStandardItemModel(this);
    filter_ = new QSortFilterProxyModel(this);
    filter_->setSourceModel(model_);
    filter_->setFilterRole(UserLoginRole);
    filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    list_ = new QListView;
    list_->setObjectName("chatterList");
    list_->setModel(filter_);
    list_->setUniformItemSizes(true);
    list_->setTextElideMode(Qt::ElideRight);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(list_, 1);
    empty_ = new QLabel;
    empty_->setObjectName("chatterEmpty");
    empty_->setAlignment(Qt::AlignCenter);
    empty_->setWordWrap(true);
    layout->addWidget(empty_, 1);
    retry_ = new QPushButton(tr("Refresh"));
    retry_->setObjectName("chatterRetry");
    layout->addWidget(retry_, 0, Qt::AlignRight);
    QFile style(":/choppa/dialog.qss");
    if (style.open(QFile::ReadOnly))
        getLayoutContainer()->setStyleSheet(QString::fromUtf8(style.readAll()) +
                                            R"(
            QListView { background: #111111; color: #eeeeee; border: none; outline: none; }
            QListView::item { height: 28px; padding: 0 8px; border: 1px solid transparent; }
            QListView::item:hover { background: #292929; }
            QListView::item:selected { background: #202020; border-color: #888888; }
            QLabel { color: #a0a0a0; }
        )");
    // Reuse rows while searching instead of allocating a second list of users.
    connect(search_, &QLineEdit::textChanged, this,
            [this](const QString &text) {
                filter_->setFilterFixedString(text.trimmed());
                updateResultState();
            });
    connect(retry_, &QPushButton::clicked, this, &ChatterListWidget::reload);
    connect(list_, &QListView::activated, this,
            [this](const QModelIndex &index) {
                const auto login = index.data(UserLoginRole).toString();
                if (!login.isEmpty())
                    Q_EMIT userClicked(login);
            });
    HotkeyController::HotkeyMap actions{
        {"delete",
         [this](const std::vector<QString> &) -> QString {
             close();
             return {};
         }},
        {"reject",
         [this](const std::vector<QString> &) -> QString {
             close();
             return {};
         }},
        {"accept", nullptr},
        {"scrollPage", nullptr},
        {"openTab", nullptr},
        {"search",
         [this](const std::vector<QString> &) -> QString {
             search_->setFocus();
             search_->selectAll();
             return {};
         }},
    };
    getApp()->getHotkeys()->shortcutsForCategory(HotkeyCategory::PopupWindow,
                                                 actions, this);
    reload();
    search_->setFocus();
}

void ChatterListWidget::reload()
{
    if (loading_ || !retry_->isEnabled())
        return;
    if (!canLoad_)
    {
        status_->setText(tr("Twitch only makes this list available to the "
                            "broadcaster and moderators."));
        retry_->setEnabled(false);
        retry_->setToolTip(tr("Moderator permissions are required."));
        updateResultState();
        return;
    }
    if (channelID_.isEmpty())
    {
        fail(tr("Channel information is not available yet. Reopen this list "
                "after the channel connects."));
        retry_->setEnabled(false);
        retry_->setToolTip(tr("Waiting for the channel to connect."));
        return;
    }
    loading_ = true;
    failed_ = false;
    retry_->setEnabled(false);
    status_->setText(tr("Loading…"));
    roleWarning_.clear();
    moderators_.clear();
    vips_.clear();
    updateResultState();
    if (!isBroadcaster_)
    {
        roleWarning_ = tr(
            "Twitch only exposes moderator and VIP roles to the broadcaster.");
        loadChatters();
        return;
    }
    const QPointer<ChatterListWidget> self(this);
    auto loadVIPs = [self] {
        if (!self || self->requestToken_.isCancelled())
            return;
        getHelix()->getChannelVIPs(
            self->channelID_,
            [self](const auto &vips) {
                if (!self || self->requestToken_.isCancelled())
                    return;
                for (const auto &vip : vips)
                    self->vips_.insert(vip.userName.toLower());
                self->loadChatters();
            },
            [self](auto error, const auto &) {
                if (!self || self->requestToken_.isCancelled())
                    return;
                if (error == HelixListVIPsError::Ratelimited)
                {
                    self->fail(
                        tr("Twitch is limiting requests. Try again shortly."),
                        true);
                    return;
                }
                self->roleWarning_ =
                    tr("Some moderator or VIP roles could not be loaded.");
                self->loadChatters();
            });
    };
    getHelix()->getModerators(
        channelID_, 1000,
        [self, loadVIPs](const auto &mods) {
            if (!self || self->requestToken_.isCancelled())
                return;
            for (const auto &mod : mods)
                self->moderators_.insert(mod.userName.toLower());
            loadVIPs();
        },
        [self, loadVIPs](auto error, const auto &) {
            if (!self || self->requestToken_.isCancelled())
                return;
            if (error == HelixGetModeratorsError::Ratelimited)
            {
                self->fail(
                    tr("Twitch is limiting requests. Try again shortly."),
                    true);
                return;
            }
            self->roleWarning_ =
                tr("Some moderator or VIP roles could not be loaded.");
            loadVIPs();
        });
}

void ChatterListWidget::loadChatters()
{
    const QPointer<ChatterListWidget> self(this);
    getHelix()->getChatters(
        channelID_, getApp()->getAccounts()->twitch.getCurrent()->getUserId(),
        50000,
        [self](const HelixChatters &chatters) {
            if (self && !self->requestToken_.isCancelled())
                self->populate(chatters);
        },
        [self](auto error, const auto &) {
            if (!self || self->requestToken_.isCancelled())
                return;
            switch (error)
            {
                case HelixGetChattersError::Ratelimited:
                    self->fail(
                        tr("Twitch is limiting requests. Try again shortly."),
                        true);
                    break;
                case HelixGetChattersError::UserMissingScope:
                    self->fail(tr(
                        "Sign in again to allow access to the chatter list."));
                    break;
                case HelixGetChattersError::UserNotAuthorized:
                    self->fail(tr("Your account does not have permission to "
                                  "load this list."));
                    break;
                default:
                    self->fail(tr("Failed to load users. Check your connection "
                                  "and retry."));
                    break;
            }
        },
        requestToken_);
}

void ChatterListWidget::closeEvent(QCloseEvent *event)
{
    requestToken_.cancel();
    BaseWindow::closeEvent(event);
}

void ChatterListWidget::populate(const HelixChatters &chatters)
{
    QStringList groups[4];
    for (const auto &name : chatters.chatters)
    {
        const auto login = name.toLower();
        const int group = login == channelName_         ? 0
                          : moderators_.contains(login) ? 1
                          : vips_.contains(login)       ? 2
                                                        : 3;
        groups[group].append(login);
    }
    const QStringList labels{tr("Broadcaster"), tr("Moderators"), tr("VIPs"),
                             tr("Chatters")};
    list_->setUpdatesEnabled(false);
    model_->clear();
    QList<QStandardItem *> rows;
    rows.reserve(chatters.chatters.size() + 4);
    for (int group = 0; group < 4; ++group)
    {
        auto &users = groups[group];
        if (users.isEmpty())
            continue;
        users.sort();
        auto *heading = new QStandardItem(
            QString("%1 (%2)").arg(labels[group]).arg(users.size()));
        heading->setFlags(Qt::NoItemFlags);
        rows.append(heading);
        for (const auto &login : users)
        {
            auto *item = new QStandardItem(login);
            item->setData(login, UserLoginRole);
            item->setToolTip(login);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            rows.append(item);
        }
    }
    model_->invisibleRootItem()->appendRows(rows);
    list_->setUpdatesEnabled(true);
    loading_ = false;
    hasData_ = true;
    failed_ = false;
    status_->setText(roleWarning_);
    if (chatters.total > chatters.chatters.size())
        status_->setText(tr("Showing %1 of %2 users. ")
                             .arg(chatters.chatters.size())
                             .arg(chatters.total) +
                         roleWarning_);
    retry_->setText(tr("Refresh"));
    // Cool down manual refreshes; there is no automatic retry loop.
    QTimer::singleShot(5000, this, [this] {
        retry_->setEnabled(true);
    });
    retry_->setToolTip(tr("Refresh is available five seconds after loading."));
    updateResultState();
}

void ChatterListWidget::fail(const QString &message, bool rateLimited)
{
    loading_ = false;
    failed_ = true;
    status_->setText(message +
                     (hasData_ ? tr(" Previously loaded users are still shown.")
                               : QString()));
    retry_->setText(tr("Retry"));
    retry_->setEnabled(!rateLimited);
    retry_->setToolTip(rateLimited ? tr("Retry is available in 60 seconds.")
                                   : tr("Load the list again"));
    if (rateLimited)
        QTimer::singleShot(60000, this, [this] {
            retry_->setEnabled(true);
        });
    updateResultState();
}

void ChatterListWidget::updateResultState()
{
    const bool empty = filter_->rowCount() == 0;
    list_->setVisible(!empty);
    empty_->setVisible(empty);
    empty_->setText(loading_               ? tr("Loading…")
                    : failed_ || !canLoad_ ? tr("No users are available.")
                    : search_->text().trimmed().isEmpty()
                        ? tr("No users are currently in this chat.")
                        : tr("No users found."));
    status_->setVisible(!status_->text().isEmpty() && (!loading_ || hasData_));
}
}  // namespace chatterino
