// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/SearchPopup.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/filters/FilterSet.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "messages/MessageElement.hpp"
#include "messages/search/AuthorPredicate.hpp"
#include "messages/search/BadgePredicate.hpp"
#include "messages/search/ChannelPredicate.hpp"
#include "messages/search/LinkPredicate.hpp"
#include "messages/search/MessageFlagsPredicate.hpp"
#include "messages/search/RegexPredicate.hpp"
#include "messages/search/SubstringPredicate.hpp"
#include "messages/search/SubtierPredicate.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/splits/Split.hpp"

#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>

namespace chatterino {

SearchPopup::SearchPopup(QWidget *parent, Split *split)
    : BasePopup(
          {
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
              BaseWindow::EnableCustomFrame,
              BaseWindow::ContentChrome,
          },
          parent)
    , split_(split)
{
    this->initLayout();
    this->setObjectName("choppaSearch");
    this->getLayoutContainer()->setObjectName("searchContent");
    this->setStyleSheet(this->styleSheet() + QStringLiteral(R"(
        #choppaSearch, #choppaSearch #searchContent { background: #111111; }
        #choppaSearch QLineEdit { font: 700 12px 'Satoshi'; color: #eeeeee; background: #1a1a1a; border: 1px solid #383838; border-radius: 6px; padding: 6px 8px; selection-background-color: #444444; }
        #choppaSearch QLineEdit:focus { border-color: #888888; }
        #choppaSearch QLabel#searchStatus { color: #a0a0a0; background: transparent; padding: 0 8px 6px; }
    )"));
    this->SearchTimer.setSingleShot(true);
    connect(&this->SearchTimer, &QTimer::timeout, this,
            &SearchPopup::ContinueSearch);
    if (this->split_ && this->split_->getChannelView().hasSelection())
    {
        this->searchInput_->setText(
            this->split_->getChannelView().getSelectedText().trimmed());
        this->searchInput_->selectAll();
    }
    this->resize(400, 600);
    this->addShortcuts();

    this->themeChangedEvent();
}

SearchPopup::~SearchPopup() = default;

void SearchPopup::addShortcuts()
{
    HotkeyController::HotkeyMap actions{
        {"search",
         [this](const std::vector<QString> &) -> QString {
             this->searchInput_->setFocus();
             this->searchInput_->selectAll();
             return "";
         }},
        {"delete",
         [this](const std::vector<QString> &) -> QString {
             this->close();
             return "";
         }},

        {"reject", nullptr},
        {"accept", nullptr},
        {"openTab", nullptr},
        {"scrollPage", nullptr},
    };

    this->shortcuts_ = getApp()->getHotkeys()->shortcutsForCategory(
        HotkeyCategory::PopupWindow, actions, this);
}

void SearchPopup::addChannel(ChannelView &channel)
{
    if (this->searchChannels_.empty())
    {
        this->channelView_->setSourceChannel(channel.underlyingChannel());
        this->channelName_ = channel.underlyingChannel()->getName();
    }
    else if (this->searchChannels_.size() == 1)
    {
        this->channelView_->setSourceChannel(
            std::make_shared<Channel>("multichannel", Channel::Type::None));

        auto flags = this->channelView_->getFlags();
        flags.set(MessageElementFlag::ChannelName);
        flags.unset(MessageElementFlag::ModeratorTools);
        this->channelView_->setOverrideFlags(flags);
    }

    this->searchChannels_.append(std::ref(channel));
    this->snapshot_.clear();

    this->updateWindowTitle();
}

void SearchPopup::goToMessage(const MessagePtr &message)
{
    for (const auto &view : this->searchChannels_)
    {
        const auto type = view.get().underlyingChannel()->getType();
        if (type == Channel::Type::TwitchMentions ||
            type == Channel::Type::TwitchAutomod)
        {
            getApp()->getWindows()->scrollToMessage(message);
            return;
        }

        if (view.get().scrollToMessage(message))
        {
            return;
        }
    }
}

void SearchPopup::goToMessageId(const QString &messageId)
{
    for (const auto &view : this->searchChannels_)
    {
        if (view.get().scrollToMessageId(messageId))
        {
            return;
        }
    }
}

void SearchPopup::updateWindowTitle()
{
    QString historyName;

    if (this->searchChannels_.size() > 1)
    {
        historyName = "multiple channels'";
    }
    else if (this->channelName_ == "/automod")
    {
        historyName = "automod";
    }
    else if (this->channelName_ == "/mentions")
    {
        historyName = "mentions";
    }
    else if (this->channelName_ == "/whispers")
    {
        historyName = "whispers";
    }
    else if (this->channelName_.isEmpty())
    {
        historyName = "<empty>'s";
    }
    else
    {
        historyName = QString("%1's").arg(this->channelName_);
    }
    this->setWindowTitle("Searching in " + historyName + " history");
}

void SearchPopup::showEvent(QShowEvent *e)
{
    this->search();
    BaseWindow::showEvent(e);
}

void SearchPopup::hideEvent(QHideEvent *Event)
{
    this->SearchTimer.stop();
    this->SearchResults.reset();
    this->SearchPredicates.clear();
    BasePopup::hideEvent(Event);
}

bool SearchPopup::eventFilter(QObject *object, QEvent *event)
{
    if (object == this->searchInput_ && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent == QKeySequence::DeleteStartOfWord &&
            this->searchInput_->selectionLength() > 0)
        {
            this->searchInput_->backspace();
            return true;
        }
    }
    return false;
}

void SearchPopup::themeChangedEvent()
{
    BasePopup::themeChangedEvent();

    this->setPalette(getTheme()->palette);
}

void SearchPopup::search()
{
    if (this->snapshot_.size() == 0)
    {
        this->snapshot_ = this->buildSnapshot();
    }

    this->SearchTimer.stop();
    this->SearchPredicates = parsePredicates(this->searchInput_->text());
    this->SearchIndex = 0;
    this->SearchMatches = 0;
    this->SearchResults =
        std::make_shared<Channel>(this->channelName_, Channel::Type::None);
    this->SearchStatus->setText(tr("Searching"));
    this->SearchTimer.start(0);
}

void SearchPopup::ContinueSearch()
{
    if (!this->SearchResults)
        return;
    QElapsedTimer Budget;
    Budget.start();
    while (this->SearchIndex < this->snapshot_.size())
    {
        const auto &Message = this->snapshot_[this->SearchIndex++];
        if (std::all_of(this->SearchPredicates.begin(),
                        this->SearchPredicates.end(),
                        [&Message](const auto &Predicate) {
                            return Predicate->appliesTo(*Message);
                        }))
        {
            auto Flags = std::optional<MessageFlags>(Message->flags);
            Flags->set(MessageFlag::DoNotLog);
            this->SearchResults->addMessage(Message, MessageContext::Repost,
                                            Flags);
            ++this->SearchMatches;
        }
        if (this->SearchIndex % 64 == 0 && Budget.elapsed() >= 4)
        {
            this->SearchTimer.start(0);
            return;
        }
    }
    this->channelView_->setChannel(this->SearchResults);
    const auto Retained = this->SearchResults->countMessages();
    this->SearchStatus->setText(
        this->SearchMatches == 0 ? tr("No matching messages")
        : Retained < this->SearchMatches
            ? tr("%1 matches, latest %2 shown")
                  .arg(this->SearchMatches)
                  .arg(Retained)
            : tr("%1 matching messages").arg(this->SearchMatches));
    this->SearchResults.reset();
    this->SearchPredicates.clear();
}

std::vector<MessagePtr> SearchPopup::buildSnapshot()
{
    // no point in filtering/sorting if it's a single channel search
    if (this->searchChannels_.length() == 1)
    {
        const auto channelPtr = this->searchChannels_.at(0);
        return channelPtr.get().channel()->getMessageSnapshot();
    }

    auto combinedSnapshot = std::vector<std::shared_ptr<const Message>>{};
    QSet<QString> SeenIds;
    for (auto &channel : this->searchChannels_)
    {
        ChannelView &sharedView = channel.get();

        const FilterSetPtr filterSet = sharedView.getFilterSet();
        std::vector<MessagePtr> snapshot =
            sharedView.channel()->getMessageSnapshot();

        for (const auto &message : snapshot)
        {
            if (filterSet &&
                !filterSet->filter(message, sharedView.underlyingChannel()))
            {
                continue;
            }

            if (!message->id.isEmpty())
            {
                if (SeenIds.contains(message->id))
                    continue;
                SeenIds.insert(message->id);
            }

            combinedSnapshot.push_back(message);
        }
    }

    // resort by time for presentation
    std::sort(combinedSnapshot.begin(), combinedSnapshot.end(),
              [](MessagePtr &a, MessagePtr &b) {
                  return a->serverReceivedTime < b->serverReceivedTime;
              });

    return combinedSnapshot;
}

void SearchPopup::initLayout()
{
    // VBOX
    {
        auto *layout1 = new QVBoxLayout(this->getLayoutContainer());
        layout1->setContentsMargins(0, 0, 0, 0);
        layout1->setSpacing(0);

        // HBOX
        {
            auto *layout2 = new QHBoxLayout();
            layout2->setContentsMargins(8, 8, 8, 8);
            layout2->setSpacing(8);

            // SEARCH INPUT
            {
                this->searchInput_ = new QLineEdit(this);
                layout2->addWidget(this->searchInput_);

                this->searchInput_->setPlaceholderText("Type to search");
                this->searchInput_->setAccessibleName(tr("Search messages"));
                this->searchInput_->setClearButtonEnabled(true);
                this->searchInput_->findChild<QAbstractButton *>()->setIcon(
                    QPixmap(":/buttons/clearSearch.png"));
                QObject::connect(this->searchInput_, &QLineEdit::textChanged,
                                 this, &SearchPopup::search);
                this->searchInput_->installEventFilter(this);
            }

            layout1->addLayout(layout2);
            this->SearchStatus = new QLabel(this);
            this->SearchStatus->setObjectName("searchStatus");
            this->SearchStatus->setAccessibleName(tr("Search status"));
            layout1->addWidget(this->SearchStatus);
        }

        // CHANNELVIEW
        {
            this->channelView_ = new ChannelView(
                this, this->split_, ChannelView::Context::Search,
                getSettings()->scrollbackSplitLimit);

            layout1->addWidget(this->channelView_, 1);
        }
    }

    this->searchInput_->setFocus();
}

std::vector<std::unique_ptr<MessagePredicate>> SearchPopup::parsePredicates(
    const QString &input)
{
    // This regex captures all name:value predicate pairs into named capturing
    // groups and matches all other inputs seperated by spaces as normal
    // strings.
    // It also ignores whitespaces in values when being surrounded by quotation
    // marks, to enable inputs like this => regex:"kappa 123"
    static QRegularExpression predicateRegex(
        R"lit((?<negation>[!\-])?(?:(?<name>\w+):(?<value>".+?"|[^\s]+))|[^\s]+?(?=$|\s))lit");
    static QRegularExpression trimQuotationMarksRegex(R"(^"|"$)");

    QRegularExpressionMatchIterator it = predicateRegex.globalMatch(input);

    std::vector<std::unique_ptr<MessagePredicate>> predicates;

    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();

        QString name = match.captured("name");
        bool isNegated = !match.captured("negation").isEmpty();
        QString value = match.captured("value");
        value.remove(trimQuotationMarksRegex);

        // match predicates

        if (name == "from")
        {
            predicates.push_back(
                std::make_unique<AuthorPredicate>(value, isNegated));
        }
        else if (name == "badge")
        {
            predicates.push_back(
                std::make_unique<BadgePredicate>(value, isNegated));
        }
        else if (name == "subtier")
        {
            predicates.push_back(
                std::make_unique<SubtierPredicate>(value, isNegated));
        }
        else if (name == "has" && value == "link")
        {
            predicates.push_back(std::make_unique<LinkPredicate>(isNegated));
        }
        else if (name == "in")
        {
            predicates.push_back(
                std::make_unique<ChannelPredicate>(value, isNegated));
        }
        else if (name == "is")
        {
            predicates.push_back(
                std::make_unique<MessageFlagsPredicate>(value, isNegated));
        }
        else if (name == "regex")
        {
            predicates.push_back(
                std::make_unique<RegexPredicate>(value, isNegated));
        }
        else
        {
            predicates.push_back(
                std::make_unique<SubstringPredicate>(match.captured()));
        }
    }

    return predicates;
}

}  // namespace chatterino
