// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/CancellationToken.hpp"
#include "widgets/BaseWindow.hpp"

#include <QSet>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;

namespace chatterino {

class TwitchChannel;
struct HelixChatters;

class ChatterListWidget : public BaseWindow
{
    Q_OBJECT

public:
    ChatterListWidget(const TwitchChannel *twitchChannel, QWidget *parent);

    Q_SIGNAL void userClicked(QString userLogin);

private:
    void closeEvent(QCloseEvent *event) override;
    void reload();
    CancellationToken requestToken_{false};
    ScopedCancellationToken cancelOnDestroy_{requestToken_};
    void loadChatters();
    void populate(const HelixChatters &chatters);
    void fail(const QString &message, bool rateLimited = false);
    void updateResultState();

    QString channelID_;
    QString channelName_;
    bool isBroadcaster_;
    bool canLoad_;
    bool loading_ = false;
    bool hasData_ = false;
    bool failed_ = false;
    QSet<QString> moderators_;
    QSet<QString> vips_;
    QString roleWarning_;
    QLineEdit *search_;
    QLabel *status_;
    QLabel *empty_;
    QPushButton *retry_;
    QListView *list_;
    QStandardItemModel *model_;
    QSortFilterProxyModel *filter_;
};

}  // namespace chatterino
