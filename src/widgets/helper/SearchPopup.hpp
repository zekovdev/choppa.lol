// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "ForwardDecl.hpp"
#include "widgets/BasePopup.hpp"

#include <QTimer>

#include <memory>

class QLineEdit;
class QLabel;

namespace chatterino {

class Split;
class MessagePredicate;

class SearchPopup : public BasePopup
{
public:
    SearchPopup(QWidget *parent, Split *split = nullptr);
    ~SearchPopup() override;

    virtual void addChannel(ChannelView &channel);
    void goToMessage(const MessagePtr &message);
    /**
     * This method should only be used for searches that
     * don't include a mentions channel,
     * since it will only search in the opened channels (not globally).
     * @param messageId
     */
    void goToMessageId(const QString &messageId);

protected:
    virtual void updateWindowTitle();
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void themeChangedEvent() override;

private:
    void initLayout();
    void search();
    void ContinueSearch();
    void addShortcuts() override;
    std::vector<MessagePtr> buildSnapshot();

    /**
     * @brief Checks the input for tags and registers their corresponding
     *        predicates.
     *
     * @param input the string to check for tags
     * @return a vector of MessagePredicates requested in the input
     */
    static std::vector<std::unique_ptr<MessagePredicate>> parsePredicates(
        const QString &input);

    std::vector<MessagePtr> snapshot_;
    QTimer SearchTimer;
    size_t SearchIndex = 0;
    size_t SearchMatches = 0;
    ChannelPtr SearchResults;
    std::vector<std::unique_ptr<MessagePredicate>> SearchPredicates;
    QLabel *SearchStatus = nullptr;
    QLineEdit *searchInput_{};
    ChannelView *channelView_{};
    QString channelName_{};
    Split *split_ = nullptr;
    QList<std::reference_wrapper<ChannelView>> searchChannels_;
};

}  // namespace chatterino
