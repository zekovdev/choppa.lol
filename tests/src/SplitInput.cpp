// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitInput.hpp"

#include "common/Channel.hpp"
#include "common/Literals.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/Command.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/completion/sources/EmoteSource.hpp"
#include "controllers/completion/strategies/SmartEmoteStrategy.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "controllers/spellcheck/SpellChecker.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "messages/layouts/MessageLayout.hpp"
#include "messages/MessageBuilder.hpp"
#include "mocks/BaseApplication.hpp"
#include "mocks/EmoteController.hpp"
#include "mocks/Helix.hpp"
#include "mocks/TwitchIrcServer.hpp"
#include "mocks/UserData.hpp"
#include "providers/twitch/IrcMessageHandler.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/CrashHandler.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/helper/GifTimer.hpp"
#include "singletons/helper/LoggingChannel.hpp"
#include "singletons/Logging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/AccountSwitchPopup.hpp"
#include "widgets/ChatterListWidget.hpp"
#include "widgets/ChoppaTitlebar.hpp"
#include "widgets/dialogs/EmotePopup.hpp"
#include "widgets/dialogs/LoginDialog.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/dialogs/UserInfoPopup.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/helper/ChoppaModerationActions.hpp"
#include "widgets/helper/ContainedMenu.hpp"
#include "widgets/helper/DebugPopup.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/helper/PopupGeometry.hpp"
#include "widgets/helper/ResizingTextEdit.hpp"
#include "widgets/helper/SearchPopup.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/Scrollbar.hpp"
#include "widgets/settingspages/GeneralPage.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QAbstractButton>
#include <QDebug>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QString>
#include <QStyleOptionButton>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <barrier>
#include <thread>

using namespace chatterino;
using ::testing::Exactly;

namespace {

class MockApplication : public mock::BaseApplication
{
public:
    MockApplication()
        : windowManager(this->args_, this->paths_, this->settings, this->theme,
                        this->fonts)
        , commands(this->paths_)
        , crashHandler(this->paths_)
    {
    }

    HotkeyController *getHotkeys() override
    {
        return &this->hotkeys;
    }

    ITwitchIrcServer *getTwitch() override
    {
        return &this->twitch;
    }

    WindowManager *getWindows() override
    {
        return &this->windowManager;
    }

    AccountController *getAccounts() override
    {
        return &this->accounts;
    }

    CommandController *getCommands() override
    {
        return &this->commands;
    }

    EmoteController *getEmotes() override
    {
        return &this->emotes;
    }

    CrashHandler *getCrashHandler() override
    {
        return &this->crashHandler;
    }

    IUserDataController *getUserData() override
    {
        return &this->userData;
    }
    mock::UserDataController userData;
    HotkeyController hotkeys;
    mock::MockTwitchIrcServer twitch;
    WindowManager windowManager;
    AccountController accounts;
    CommandController commands;
    mock::EmoteController emotes;
    CrashHandler crashHandler;
};

class SplitInputTest
    : public ::testing::TestWithParam<std::tuple<QString, QString>>
{
public:
    SplitInputTest()
        : split(new Split(nullptr))
        , input(this->split)
    {
    }

    MockApplication mockApplication;
    Split *split;
    SplitInput input;
};

}  // namespace

TEST_P(SplitInputTest, Reply)
{
    std::tuple<QString, QString> params = this->GetParam();
    auto [inputText, expected] = params;
    ASSERT_EQ("", this->input.getInputText());
    this->input.setInputText(inputText);
    ASSERT_EQ(inputText, this->input.getInputText());

    auto *message = new Message();
    message->displayName = "forsen";
    auto reply = MessagePtr(message);
    this->input.setReply(reply, {});
    QString actual = this->input.getInputText();
    ASSERT_EQ(expected, actual) << "Input text after setReply should be '"
                                << expected << "', but got '" << actual << "'";
}

INSTANTIATE_TEST_SUITE_P(
    SplitInput, SplitInputTest,
    testing::Values(
        // Ensure message is retained
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "Test message",
            // Expected text after replying to forsen
            "@forsen Test message "),

        // Ensure mention is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure mention with space is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen ",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure mention is stripped, retain message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen Test message",
            // Expected text after replying to forsen
            "@forsen Test message "),

        // Ensure mention with comma is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen,",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure mention with comma is stripped, retain message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen Test message",
            // Expected text after replying to forsen
            "@forsen Test message "),

        // Ensure mention with comma and space is stripped, no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "@forsen, ",
            // Expected text after replying to forsen
            "@forsen "),

        // Ensure it works with no message
        std::make_tuple<QString, QString>(
            // Pre-existing text in the input
            "",
            // Expected text after replying to forsen
            "@forsen ")));

// External navigation must preserve page identity, selection and close behavior.
TEST(ChoppaNavigation, SwitchingAndClosingPreservesChatPages)
{
    MockApplication app;
    Notebook notebook(nullptr);
    notebook.resize(800, 500);
    notebook.setExternalNavigation(true);
    auto *first = new QWidget(&notebook);
    auto *second = new QWidget(&notebook);
    auto *firstTab = notebook.addPage(first, "first", true);
    auto *secondTab = notebook.addPage(second, "second", false);
    notebook.show();
    notebook.refresh();
    EXPECT_TRUE(firstTab->isHidden());
    EXPECT_TRUE(secondTab->isHidden());
    EXPECT_EQ(first->geometry(), notebook.rect());
    notebook.selectIndex(1);
    EXPECT_EQ(notebook.getSelectedPage(), second);
    EXPECT_TRUE(first->isHidden());
    EXPECT_EQ(second->geometry(), notebook.rect());
    EXPECT_EQ(notebook.getVisibleTabCount(), 2);
    EXPECT_TRUE(notebook.isPageVisible(0));
    EXPECT_FALSE(notebook.isPageVisible(-1));
    EXPECT_FALSE(notebook.isPageVisible(2));
    notebook.rearrangePage(second, 0);
    EXPECT_EQ(notebook.getSelectedPage(), second);
    EXPECT_EQ(notebook.getSelectedIndex(), 0);
    EXPECT_EQ(notebook.getPageAt(1), first);
    notebook.setTabLocation(NotebookTabLocation::Left);
    EXPECT_EQ(second->geometry(), notebook.rect());
    notebook.removePage(second);
    EXPECT_EQ(notebook.getSelectedPage(), first);
    EXPECT_EQ(notebook.getPageCount(), 1);
    EXPECT_EQ(notebook.getPageAt(0), first);
    EXPECT_TRUE(firstTab->isHidden());
}

TEST(ChoppaNavigation, CompactChatTabsReorderAndUseConsistentControls)
{
    MockApplication app;
    Window window(WindowType::Main, nullptr);
    window.resize(920, 520);
    auto &notebook = window.getNotebook();
    auto *first = notebook.addPage(true);
    auto *second = notebook.addPage(true);
    auto *third = notebook.addPage(true);
    first->getTab()->setCustomTitle("one");
    second->getTab()->setCustomTitle("two");
    third->getTab()->setCustomTitle("three");
    window.show();
    QApplication::processEvents();

    auto *tabs = window.findChild<QListWidget *>("choppaChatTabs");
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->dragDropMode(), QAbstractItemView::InternalMove);
    EXPECT_EQ(tabs->movement(), QListView::Snap);
    EXPECT_TRUE(tabs->dragEnabled());
    EXPECT_TRUE(tabs->acceptDrops());
    static_cast<Notebook &>(notebook).setLockNotebookLayout(true);
    const auto from = tabs->visualItemRect(tabs->item(2)).center();
    const auto to = tabs->visualItemRect(tabs->item(0)).center();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(from),
                      QPointF(tabs->viewport()->mapToGlobal(from)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tabs->viewport(), &press);
    QMouseEvent drag(QEvent::MouseMove, QPointF(to),
                     QPointF(tabs->viewport()->mapToGlobal(to)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tabs->viewport(), &drag);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(to),
                        QPointF(tabs->viewport()->mapToGlobal(to)),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tabs->viewport(), &release);
    QApplication::processEvents();
    EXPECT_EQ(notebook.getPageAt(0), third);
    EXPECT_EQ(notebook.getPageAt(1), first);
    EXPECT_EQ(notebook.getPageAt(2), second);

    QPushButton *split = nullptr;
    QPushButton *channel = nullptr;
    bool hasStreamerMode = false;
    for (auto *button : window.findChildren<QPushButton *>())
    {
        if (button->text() == "+ Split")
            split = button;
        else if (button->text() == "+ Channel")
            channel = button;
        if (button->text().contains("Streamer mode", Qt::CaseInsensitive))
            hasStreamerMode = true;
    }
    ASSERT_NE(split, nullptr);
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(split->objectName(), channel->objectName());
    EXPECT_FALSE(hasStreamerMode);
    EXPECT_EQ(QApplication::font().family(), "Satoshi");
    EXPECT_EQ(QApplication::font().weight(), QFont::Bold);
}

TEST(ChoppaNavigation, AccountSwitcherStaysInsideWindowAndFitsItsRows)
{
    MockApplication app;
    QWidget host;
    host.setGeometry(40, 40, 640, 420);
    host.show();
    QApplication::processEvents();
    app.windowManager.showAccountSelectPopup(
        host.mapToGlobal(QPoint(12, host.height() - 12)), &host);
    QApplication::processEvents();

    AccountSwitchPopup *popup = nullptr;
    for (auto *widget : QApplication::topLevelWidgets())
        if ((popup = dynamic_cast<AccountSwitchPopup *>(widget)))
            break;
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(popupAvailableRect(&host).contains(popup->geometry()));
    for (auto *list : popup->findChildren<QListWidget *>())
    {
        EXPECT_EQ(list->verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
        EXPECT_EQ(list->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    }
    popup->hide();
}

TEST(ChoppaNavigation, UnreadAndTitleChangesReachNavigation)
{
    MockApplication app;
    Notebook notebook(nullptr);
    notebook.setExternalNavigation(true);
    auto *page = new QWidget(&notebook);
    auto *tab = notebook.addPage(page, "first", true);
    int notifications = 0;
    QObject::connect(&notebook, &Notebook::navigationChanged, &notebook,
                     [&notifications] {
                         ++notifications;
                     });
    tab->setHighlightState(HighlightState::Highlighted);
    EXPECT_EQ(notifications, 0);  // the selected workspace stays read
    notebook.addPage(new QWidget(&notebook), "active", true);
    notifications = 0;
    tab->setHighlightState(HighlightState::Highlighted);
    EXPECT_GT(notifications, 0);
    notifications = 0;
    tab->setCustomTitle("renamed");
    EXPECT_GT(notifications, 0);
    EXPECT_EQ(tab->getTitle(), "renamed");
    EXPECT_EQ(notebook.getPageAt(0), page);
}

TEST(ChoppaNavigation, LiveSurvivesUnreadMentionAndMarkRead)
{
    MockApplication app;
    Notebook notebook(nullptr);
    notebook.setExternalNavigation(true);
    auto *page = new QWidget(&notebook);
    auto *tab = notebook.addPage(page, "live", true);
    notebook.addPage(new QWidget(&notebook), "active", true);
    int updates = 0;
    QObject::connect(&notebook, &Notebook::navigationChanged, &notebook, [&] {
        ++updates;
    });
    tab->setLive(true);
    EXPECT_GT(updates, 0);
    tab->setHighlightState(HighlightState::NewMessage);
    EXPECT_TRUE(tab->isLive());
    tab->setHighlightState(HighlightState::Highlighted);
    EXPECT_TRUE(tab->isLive());
    tab->setHighlightState(HighlightState::NewMessage);
    EXPECT_EQ(tab->highlightState(), HighlightState::Highlighted);
    notebook.select(page);
    EXPECT_EQ(tab->highlightState(), HighlightState::None);
    EXPECT_TRUE(tab->isLive());
    tab->setLive(false);
    EXPECT_FALSE(tab->isLive());
}

TEST(ChoppaNavigation, CloseGlyphIsCenteredAndClosesItsWindow)
{
    QWidget window;
    window.setStyleSheet(
        "QPushButton { text-align: left; padding-left: 12px; }");
    auto *layout = new QVBoxLayout(&window);
    auto *titlebar = new ChoppaTitlebar(&window);
    layout->addWidget(titlebar);
    window.show();
    auto *close = titlebar->findChild<QAbstractButton *>("close");
    ASSERT_NE(close, nullptr);
    close->clearFocus();
    QImage image(close->size(), QImage::Format_ARGB32);
    image.fill(Qt::black);
    close->render(&image);
    QRect glyph;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qRed(image.pixel(x, y)) > 180 &&
                qGreen(image.pixel(x, y)) > 180)
                glyph = glyph.united(QRect(x, y, 1, 1));
    ASSERT_FALSE(glyph.isEmpty());
    EXPECT_NEAR(glyph.center().x(), (image.width() - 1) / 2.0, 1);
    EXPECT_NEAR(glyph.center().y(), (image.height() - 1) / 2.0, 1);
    close->click();
    EXPECT_FALSE(window.isVisible());
}

TEST(ChoppaNavigation, RestoredWindowSavesCurrentBounds)
{
    MockApplication app;
    BaseWindow window({BaseWindow::EnableCustomFrame, BaseWindow::ContentChrome,
                       BaseWindow::DisableLayoutSave});
    window.setGeometry(80, 80, 780, 510);
    window.show();
    QApplication::processEvents();
    window.setGeometry(90, 100, 760, 490);
    // Saving must not depend on a queued native resize-cache update.
    EXPECT_EQ(window.getBounds(), window.geometry());
}

TEST(ChoppaNavigation, SnapshotCacheTracksPauseReplacementAndChannelSwitch)
{
    MockApplication App;
    ChannelView View(nullptr, ChannelView::Context::None, 3);
    auto Source = std::make_shared<Channel>("cache", Channel::Type::Misc);
    std::vector<MessagePtr> Messages;
    for (int Index = 0; Index < 5; ++Index)
    {
        MessageBuilder Builder;
        Builder.message().id = QString::number(Index);
        Builder.emplace<TextElement>(QString::number(Index),
                                     MessageElementFlag::Text);
        Messages.push_back(Builder.release());
    }
    for (int Index = 0; Index < 3; ++Index)
        Source->addMessage(Messages[Index], MessageContext::Repost);
    View.setChannel(Source);
    View.setPausable(true);
    View.resize(500, 180);
    View.show();
    QApplication::processEvents();
    ASSERT_EQ(View.getMessagesSnapshot().size(), 3);
    EXPECT_EQ(View.getMessagesSnapshot().front()->getMessagePtr(), Messages[0]);
    View.pause(PauseReason::Selection);
    Source->addMessage(Messages[3], MessageContext::Repost);
    EXPECT_EQ(View.getMessagesSnapshot().front()->getMessagePtr(), Messages[0]);
    View.unpause(PauseReason::Selection);
    EXPECT_EQ(View.getMessagesSnapshot().front()->getMessagePtr(), Messages[1]);
    Source->replaceMessage(3, Messages[4]);
    EXPECT_EQ(View.getMessagesSnapshot().back()->getMessagePtr(), Messages[4]);
    auto Other = std::make_shared<Channel>("other", Channel::Type::Misc);
    Other->addMessage(Messages[0], MessageContext::Repost);
    View.setChannel(Other);
    ASSERT_EQ(View.getMessagesSnapshot().size(), 1);
    EXPECT_EQ(View.getMessagesSnapshot().front()->getMessagePtr(), Messages[0]);
    View.clearMessages();
    EXPECT_TRUE(View.getMessagesSnapshot().empty());
    QWheelEvent Wheel(QPointF(20, 20), QPointF(20, 20), QPoint(),
                      QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(&View, &Wheel);
    EXPECT_TRUE(View.getMessagesSnapshot().empty());
}

TEST(ChoppaNavigation, SearchUsesLatestQueryAndCanCloseDuringFiltering)
{
    MockApplication App;
    const auto HistoryLimit = App.settings.scrollbackSplitLimit.getValue();
    const auto RestoreLimit = qScopeGuard([&] {
        App.settings.scrollbackSplitLimit = HistoryLimit;
    });
    App.settings.scrollbackSplitLimit = 16000;
    auto Source = std::make_shared<Channel>("search", Channel::Type::Misc);
    for (int Index = 0; Index < 16000; ++Index)
    {
        MessageBuilder Builder;
        const auto Text = Index % 16 == 0 ? "final" : "previous";
        Builder.message().searchText = Text;
        Builder.message().messageText = Text;
        Builder.message().id = QString::number(Index);
        Builder.emplace<TextElement>(Text, MessageElementFlag::Text);
        Source->addMessage(Builder.release(), MessageContext::Repost);
    }
    ChannelView Original(nullptr, ChannelView::Context::None, 16000);
    Original.setChannel(Source);
    SearchPopup Popup(nullptr);
    Popup.addChannel(Original);
    Popup.show();
    auto *Input = Popup.findChild<QLineEdit *>();
    auto *Status = Popup.findChild<QLabel *>("searchStatus");
    auto *Results = Popup.findChild<ChannelView *>();
    ASSERT_NE(Input, nullptr);
    ASSERT_NE(Status, nullptr);
    ASSERT_NE(Results, nullptr);
    Input->setText("previous");
    Input->setText("final");
    QElapsedTimer Timeout;
    Timeout.start();
    while (Status->text() == "Searching" && Timeout.elapsed() < 3000)
        QApplication::processEvents();
    EXPECT_EQ(Status->text(), "1000 matching messages");
    EXPECT_LE(Status->height(), 40);
    EXPECT_LE(Input->height(), 48);
    EXPECT_GE(Results->height(), Popup.height() * 3 / 4);
    const auto Snapshot = Results->channel()->getMessageSnapshot();
    ASSERT_EQ(Snapshot.size(), 1000);
    for (const auto &Message : Snapshot)
        EXPECT_EQ(Message->searchText, "final");
    Input->setText("regex:previous");
    Popup.close();
    QApplication::processEvents();
    EXPECT_FALSE(Popup.isVisible());
    Popup.show();
    Timeout.restart();
    while (Status->text() == "Searching" && Timeout.elapsed() < 3000)
        QApplication::processEvents();
    EXPECT_EQ(Status->text(), "15000 matching messages");
    const auto Captures = qEnvironmentVariable("CHOPPA_MEDIA_DIRECTORY");
    if (!Captures.isEmpty())
        EXPECT_TRUE(Popup.grab().save(Captures + "/search.png"));
    Popup.close();
}

TEST(ChoppaNavigation, ClosedWidgetsAndTimersDetachSettingsCallbacks)
{
    MockApplication App;
    const auto Prefix = App.settings.prefixOnlyEmoteCompletion.getValue();
    const auto Animate = App.settings.animateEmotes.getValue();
    const auto Cache = App.settings.cachePath.getValue();
    const auto Search = App.settings.searchEnabled.getValue();
    const auto Restore = qScopeGuard([&] {
        App.settings.prefixOnlyEmoteCompletion = Prefix;
        App.settings.animateEmotes = Animate;
        App.settings.cachePath = Cache;
        App.settings.searchEnabled = Search;
    });
    QTemporaryDir Directory;
    for (int Index = 0; Index < 8; ++Index)
    {
        {
            ResizingTextEdit Edit;
            GeneralPage Page;
            GIFTimer Timer;
            Timer.initialize();
        }
        App.settings.prefixOnlyEmoteCompletion = Index % 2 == 0;
        App.settings.animateEmotes = Index % 2 == 0;
        App.settings.searchEnabled = Index % 2 == 0;
        App.settings.cachePath = Directory.filePath(QString::number(Index));
        QApplication::processEvents();
    }
}

TEST(ChoppaNavigation, PendingLayoutSaveCancelsOnShutdown)
{
    QPointer<QTimer> Pending;
    {
        MockApplication App;
        Pending = App.windowManager.findChild<QTimer *>();
        ASSERT_NE(Pending, nullptr);
        Pending->start(0);
    }
    QApplication::processEvents();
    EXPECT_TRUE(Pending.isNull());
}

TEST(ChoppaNavigation, BufferedLogsFlushOnCloseAndDisconnectSettings)
{
    MockApplication App;
    QTemporaryDir Directory;
    App.settings.logPath = Directory.path();
    App.settings.enableLogging = true;
    Logging Logger(App.settings);
    for (int Index = 0; Index < 1000; ++Index)
    {
        MessageBuilder Builder;
        Builder.message().messageText = QString("logged-%1").arg(Index);
        Logger.addMessage("quality", Builder.release(), "twitch", "");
    }
    Logger.closeChannel("quality", "twitch");
    QDirIterator Logs(Directory.path(), {"*.log"}, QDir::Files,
                      QDirIterator::Subdirectories);
    ASSERT_TRUE(Logs.hasNext());
    QFile Log(Logs.next());
    ASSERT_TRUE(Log.open(QIODevice::ReadOnly));
    const auto Content = Log.readAll();
    for (int Index = 0; Index < 1000; ++Index)
        EXPECT_TRUE(
            Content.contains(QString("logged-%1\n").arg(Index).toUtf8()));
    EXPECT_TRUE(Content.contains("# Stop logging"));
    QFile Blocked(Directory.filePath("blocked"));
    ASSERT_TRUE(Blocked.open(QIODevice::WriteOnly));
    Blocked.close();
    App.settings.logPath = Blocked.fileName();
    Logger.addMessage("unwritable", makeSystemMessage("failed path"), "twitch",
                      "");
    Logger.closeChannel("unwritable", "twitch");
    App.settings.logPath = Directory.path();
    App.settings.enableLogging = false;
}

TEST(ChoppaNavigation, PixmapCacheUsesContentIdentityAndReleasesImages)
{
    MockApplication App;
    QPixmap Pixmap(12, 12);
    Pixmap.fill(Qt::red);
    auto Red = Image::fromResourcePixmap(Pixmap);
    EXPECT_EQ(Image::fromResourcePixmap(Pixmap), Red);
    QPixmap Copy = Pixmap;
    EXPECT_EQ(Image::fromResourcePixmap(Copy), Red);
    Pixmap.fill(Qt::blue);
    auto Blue = Image::fromResourcePixmap(Pixmap);
    EXPECT_NE(Blue, Red);
    ASSERT_TRUE(Red->pixmapOrLoad().has_value());
    ASSERT_TRUE(Blue->pixmapOrLoad().has_value());
    EXPECT_EQ(Red->pixmapOrLoad()->toImage().pixelColor(0, 0), QColor(Qt::red));
    EXPECT_EQ(Blue->pixmapOrLoad()->toImage().pixelColor(0, 0),
              QColor(Qt::blue));
    std::weak_ptr<Image> Released;
    for (int Index = 0; Index < 20000; ++Index)
    {
        auto Cached = Image::fromUrl(
            Url{QString("https://example.invalid/cache-%1").arg(Index)});
        EXPECT_EQ(Image::fromUrl(Cached->url()), Cached);
        Released = Cached;
    }
    EXPECT_TRUE(Released.expired());
}

#ifndef DISABLE_IMAGE_EXPIRATION_POOL
TEST(ChoppaNavigation, ImageCleanupDuringConcurrentOwnershipRelease)
{
    MockApplication App;
    auto &Pool = ImageExpirationPool::instance();
    for (int Round = 0; Round < 20; ++Round)
    {
        std::vector<ImagePtr> Images;
        std::vector<std::weak_ptr<Image>> Released;
        for (int Index = 0; Index < 256; ++Index)
        {
            auto Image = Image::fromUrl(
                Url{QString("https://example.invalid/cleanup-%1-%2")
                        .arg(Round)
                        .arg(Index)});
            Pool.addImagePtr(Image);
            Released.push_back(Image);
            Images.push_back(std::move(Image));
        }
        std::barrier Start(2);
        std::thread Worker([Owned = std::move(Images), &Start]() mutable {
            Start.arrive_and_wait();
            Owned.clear();
        });
        Start.arrive_and_wait();
        Pool.freeOld();
        Worker.join();
        Pool.freeOld();
        for (const auto &Image : Released)
            EXPECT_TRUE(Image.expired());
        QApplication::processEvents();
    }
    Pool.freeAll();
}
#endif

TEST(ChoppaNavigation, SustainedMessageLoadReleasesEvictedHistory)
{
    MockApplication App;
    const auto HistoryLimit = App.settings.scrollbackSplitLimit.getValue();
    const auto RestoreLimit = qScopeGuard([&] {
        App.settings.scrollbackSplitLimit = HistoryLimit;
    });
    App.settings.scrollbackSplitLimit = 1000;
    auto Channel =
        std::make_shared<chatterino::Channel>("sustained", Channel::Type::Misc);
    ChannelView View(nullptr, ChannelView::Context::None, 1000);
    View.resize(640, 360);
    View.setOverrideFlags(MessageElementFlag::Text);
    View.setChannel(Channel);
    View.show();
    std::vector<std::weak_ptr<const Message>> Evicted;
    for (int Index = 0; Index < 30000; ++Index)
    {
        MessageBuilder Builder;
        Builder.message().id = QString::number(Index);
        Builder.emplace<TextElement>(QString("message %1").arg(Index),
                                     MessageElementFlag::Text);
        auto Message = Builder.release();
        if (Index < 256)
            Evicted.push_back(Message);
        Channel->addMessage(Message, MessageContext::Repost);
        if (Index % 64 == 0)
            QApplication::processEvents();
    }
    QApplication::processEvents();
    EXPECT_EQ(Channel->countMessages(), 1000);
    ASSERT_EQ(View.getMessagesSnapshot().size(), 1000);
    EXPECT_EQ(View.getMessagesSnapshot().back()->getMessagePtr()->id, "29999");
    for (const auto &Message : Evicted)
        EXPECT_TRUE(Message.expired());
    const auto Visible =
        std::weak_ptr<MessageLayout>(View.getMessagesSnapshot().back());
    View.clearMessages();
    QApplication::processEvents();
    EXPECT_TRUE(Visible.expired());
}

TEST(ChoppaPerformance, ChatWorkload)
{
    const auto Destination = qEnvironmentVariable("CHOPPA_PERF_RESULTS");
    if (Destination.isEmpty())
        GTEST_SKIP();
    MockApplication App;
    App.settings.enableSmoothScrolling = false;
    App.settings.enableSmoothScrollingNewMessages = false;
    std::vector<MessagePtr> Messages;
    Messages.reserve(2500);
    QPixmap Badge(18, 18);
    Badge.fill(QColor("#8b5cf6"));
    auto Image = Image::fromResourcePixmap(Badge);
    for (int Index = 0; Index < 2500; ++Index)
    {
        MessageBuilder Builder;
        Builder.message().id = QString::number(Index);
        Builder.message().messageText =
            QString("user%1: Nachricht mit Emotes, Umlauten und einem langen "
                    "Text zum Testen der Chatansicht %2")
                .arg(Index % 32)
                .arg(Index);
        Builder.message().searchText = Builder.message().messageText;
        Builder.emplace<ImageElement>(Image, MessageElementFlag::Text);
        Builder.emplace<TextElement>(QString("user%1:").arg(Index % 32),
                                     MessageElementFlag::Text,
                                     QColor("#6ecfbe"));
        Builder.emplace<TextElement>(Builder.message().messageText,
                                     MessageElementFlag::Text);
        if (Index % 3 == 0)
            Builder.emplace<ImageElement>(Image, MessageElementFlag::Text);
        Messages.push_back(Builder.release());
    }
    std::vector<std::unique_ptr<ChannelView>> Views;
    std::vector<ChannelPtr> Channels;
    for (int Index = 0; Index < 4; ++Index)
    {
        auto Channel = std::make_shared<chatterino::Channel>(
            QString("load%1").arg(Index), Channel::Type::Misc);
        auto View = std::make_unique<ChannelView>(
            nullptr, ChannelView::Context::None, 1000);
        View->resize(640, 360);
        View->setOverrideFlags(MessageElementFlag::Text);
        View->setChannel(Channel);
        for (int Row = 0; Row < 1000; ++Row)
            Channel->addMessage(Messages[Row], MessageContext::Repost);
        View->show();
        Views.push_back(std::move(View));
        Channels.push_back(std::move(Channel));
    }
    QApplication::processEvents();
    QImage Canvas(640, 360, QImage::Format_ARGB32_Premultiplied);
    for (auto &View : Views)
        View->render(&Canvas);
    QJsonObject Metrics;
    QElapsedTimer Timer;
    Timer.start();
    for (int Row = 1000; Row < 1500; ++Row)
        for (const auto &Channel : Channels)
            Channel->addMessage(Messages[Row], MessageContext::Repost);
    Metrics["Burst2000UiMs"] = Timer.nsecsElapsed() / 1e6;
    Timer.restart();
    QApplication::processEvents();
    for (auto &View : Views)
        View->render(&Canvas);
    Metrics["BurstFlushMs"] = Timer.nsecsElapsed() / 1e6;
    Timer.restart();
    size_t SnapshotSize = 0;
    for (int Index = 0; Index < 10000; ++Index)
        SnapshotSize += Views.front()->getMessagesSnapshot().size();
    Metrics["Snapshot10000Ms"] = Timer.nsecsElapsed() / 1e6;
    EXPECT_EQ(SnapshotSize, 10000000);
    std::vector<double> PaintTimes;
    for (int Frame = 0; Frame < 240; ++Frame)
    {
        Timer.restart();
        for (auto &View : Views)
            View->render(&Canvas);
        PaintTimes.push_back(Timer.nsecsElapsed() / 1e6);
    }
    std::sort(PaintTimes.begin(), PaintTimes.end());
    Metrics["FourViewsPaintMedianMs"] = PaintTimes[PaintTimes.size() / 2];
    Metrics["FourViewsPaintP95Ms"] = PaintTimes[PaintTimes.size() * 95 / 100];
    std::vector<double> ScrollTimes;
    for (int Frame = 0; Frame < 120; ++Frame)
    {
        Timer.restart();
        for (auto &View : Views)
        {
            auto &Bar = View->getScrollBar();
            Bar.setDesiredValue(Bar.getMinimum() + (Frame * 7) % 850, false);
            View->render(&Canvas);
        }
        ScrollTimes.push_back(Timer.nsecsElapsed() / 1e6);
    }
    std::sort(ScrollTimes.begin(), ScrollTimes.end());
    Metrics["FourViewsScrollMedianMs"] = ScrollTimes[ScrollTimes.size() / 2];
    Metrics["FourViewsScrollP95Ms"] =
        ScrollTimes[ScrollTimes.size() * 95 / 100];
    Metrics["Views"] = 4;
    Metrics["HistoryPerView"] = 1000;
    QFile Report(Destination);
    ASSERT_TRUE(Report.open(QIODevice::WriteOnly));
    const auto Bytes = QJsonDocument(Metrics).toJson(QJsonDocument::Indented);
    ASSERT_EQ(Report.write(Bytes), Bytes.size());
}

TEST(ChoppaPerformance, CompletionWorkload)
{
    const auto Destination = qEnvironmentVariable("CHOPPA_COMPLETION_RESULTS");
    if (Destination.isEmpty())
        GTEST_SKIP();
    MockApplication App;
    std::vector<completion::EmoteItem> Items;
    Items.reserve(4000);
    for (int Index = 0; Index < 4000; ++Index)
    {
        auto Emote = std::make_shared<chatterino::Emote>();
        Emote->zeroWidth = Index % 10 == 0;
        auto Name = QString("Keep_Emote_%1").arg(Index);
        Items.push_back({.emote = Emote,
                         .searchName = Name,
                         .tabCompletionName = Name,
                         .displayName = Name,
                         .providerName = "7TV",
                         .isEmoji = false});
    }
    completion::SmartEmoteStrategy Strategy;
    std::vector<completion::EmoteItem> Output;
    std::vector<double> Samples;
    QElapsedTimer Timer;
    for (int Index = 0; Index < 40; ++Index)
    {
        Output.clear();
        Timer.start();
        static_cast<const completion::EmoteSource::EmoteStrategy &>(Strategy)
            .apply(Items, Output, Index % 2 == 0 ? "keep" : "~keep");
        Samples.push_back(Timer.nsecsElapsed() / 1e6);
        EXPECT_EQ(Output.size(), Index % 2 == 0 ? 4000 : 400);
    }
    std::sort(Samples.begin(), Samples.end());
    QFile Report(Destination);
    ASSERT_TRUE(Report.open(QIODevice::WriteOnly));
    const auto Bytes =
        QJsonDocument(
            QJsonObject{{"CompletionMedianMs", Samples[Samples.size() / 2]},
                        {"CompletionP95Ms", Samples[Samples.size() * 95 / 100]},
                        {"Emotes", 4000}})
            .toJson(QJsonDocument::Indented);
    ASSERT_EQ(Report.write(Bytes), Bytes.size());
}

TEST(ChoppaMedia, ExportOfflineDemo)
{
    const auto directory = qEnvironmentVariable("CHOPPA_MEDIA_DIRECTORY");
    if (directory.isEmpty())
        GTEST_SKIP() << "Media export is opt-in";
    MockApplication app;
    Window window(WindowType::Main, nullptr);
    window.resize(1000, 520);
    auto &notebook = window.getNotebook();
    const QStringList names{"choppa_demo", "creative",  "music",
                            "speedruns",   "community", "development",
                            "highlights",  "mentions"};
    for (int i = 0; i < names.size(); ++i)
    {
        auto *page = notebook.addPage(i == 0);
        auto *split = page->appendNewSplit(false);
        auto channel = std::make_shared<Channel>(names[i], Channel::Type::Misc);
        const QStringList authors{"zekovdev", "pixelpilot", "midnight",
                                  "bytewave", "luna",       "community"};
        const QStringList lines{
            "hey everyone, what are we working on tonight?",
            "still fixing my keybinds",
            "does anyone have the link from earlier?",
            "check the pinned message",
            "Twitch, Kick, tabs and splits in one place",
            "these are fictional messages in an offline demo",
            "just got here, did I miss anything?",
            "we are setting up the next session",
            "sounds good, give me five minutes",
            "thanks to everyone building open-source chat tools",
            "one more tweak before calling it a night",
            "see you in chat"};
        for (int line = 0; line < lines.size(); ++line)
        {
            MessageBuilder builder;
            builder.append(std::make_unique<TextElement>(
                QString("20:%1").arg(line + 10), MessageElementFlag::Text,
                QColor("#777777")));
            builder.append(std::make_unique<TextElement>(
                authors[line % authors.size()] + ":", MessageElementFlag::Text,
                QColor(line % 2 ? "#b4b6ff" : "#6ecfbe")));
            builder.append(std::make_unique<TextElement>(
                lines[line], MessageElementFlag::Text));
            channel->addMessage(builder.release(), MessageContext::Repost);
        }
        split->setChannel(IndirectChannel(channel));
        page->getTab()->setCustomTitle(names[i]);
        page->getTab()->setLive(i == 1 || i == 3);
        split->getChannelView().setOverrideFlags(MessageElementFlag::Text);
    }
    notebook.selectIndex(0);
    notebook.getSelectedPage()->getSplits().front()->getInput().setInputText(
        "anyone up for another round?");
    window.show();
    QApplication::processEvents();
    EXPECT_TRUE(window.grab().save(directory + "/chat.png"));
    for (auto *button : window.findChildren<QPushButton *>())
    {
        if (button->accessibleName() == "Toggle sidebar")
        {
            button->click();
            QApplication::processEvents();
            EXPECT_TRUE(window.grab().save(directory + "/sidebar.png"));
            button->click();
            break;
        }
    }
    LoginDialog login(nullptr);
    login.show();
    QApplication::processEvents();
    EXPECT_TRUE(login.grab().save(directory + "/account.png"));
    login.close();
    SettingsDialog::showDialog(&window);
    QApplication::processEvents();
    bool capturedSettings = false;
    for (auto *widget : QApplication::topLevelWidgets())
    {
        if (auto *settings = dynamic_cast<SettingsDialog *>(widget))
        {
            capturedSettings =
                settings->grab().save(directory + "/settings.png");
            settings->close();
        }
    }
    EXPECT_TRUE(capturedSettings);
    QMenu moderation;
    moderation.setStyleSheet(
        "QMenu { background: #151515; color: #eeeeee; border: none; "
        "padding: 8px; font: 700 12px 'Satoshi'; } QMenu::item { padding: 8px "
        "22px; border-radius: 5px; } QMenu::item:selected { background: "
        "#303030; } QMenu::separator { height: 1px; background: #292929; "
        "margin: 5px 8px; }");
    moderation.addSection("Moderation in #choppa_demo");
    appendChoppaModerationActions(
        &moderation, false, true, [](const QString &, const QString &) {
            ADD_FAILURE() << "Capture must not execute moderation";
        });
    moderation.show();
    QApplication::processEvents();
    EXPECT_TRUE(moderation.grab().save(directory + "/moderation.png"));
    auto *timeouts = moderation.actions().at(1)->menu();
    ASSERT_NE(timeouts, nullptr);
    timeouts->setStyleSheet(moderation.styleSheet());
    timeouts->show();
    QApplication::processEvents();
    EXPECT_TRUE(timeouts->grab().save(directory + "/timeouts.png"));
    timeouts->close();
    moderation.close();
    notebook.getSelectedPage()->getTab()->showContextMenu(QPoint(40, 40));
    QApplication::processEvents();
    if (auto *popup = QApplication::activePopupWidget())
    {
        EXPECT_TRUE(popup->grab().save(directory + "/workspace-menu.png"));
        popup->close();
    }
}

TEST(ChoppaNavigation, BundledDictionariesRecognizeEnglishAndGerman)
{
#ifdef CHATTERINO_WITH_SPELLCHECK
    const auto directory = qEnvironmentVariable("CHOPPA_TEST_DICTIONARIES");
    if (directory.isEmpty())
        GTEST_SKIP() << "No packaged dictionaries supplied";
    MockApplication app;
    app.settings.spellCheckingDefaultDictionary = directory + "/de_DE";
    SpellChecker german;
    ASSERT_TRUE(german.isLoaded());
    EXPECT_TRUE(german.check(QString::fromUtf8("Grüße")));
    EXPECT_TRUE(german.check(QString::fromUtf8("Straße")));
    EXPECT_FALSE(german.check("zzqxxyyzz"));
    app.settings.spellCheckingDefaultDictionary = directory + "/en_US";
    SpellChecker english;
    ASSERT_TRUE(english.isLoaded());
    EXPECT_TRUE(english.check("community"));
    EXPECT_FALSE(english.check("commmunity"));
#else
    GTEST_SKIP() << "Spell checking is not compiled in";
#endif
}

TEST(ChoppaNavigation, ThemeRoundTripRestoresExistingMessageColors)
{
    MockApplication app;
    Split split(nullptr);
    auto &view = split.getChannelView();
    auto channel = std::make_shared<Channel>("theme-test", Channel::Type::Misc);
    MessageBuilder builder;
    builder.append(std::make_unique<TextElement>("Theme contrast regression",
                                                 MessageElementFlag::Text));
    channel->addMessage(builder.release(), MessageContext::Repost);
    view.setChannel(channel);
    view.setOverrideFlags(MessageElementFlag::Text);
    view.resize(420, 120);
    app.theme.themeName = "Choppa";
    view.show();
    QApplication::processEvents();
    const auto before = view.grab().toImage();
    app.theme.themeName = "Light";
    QApplication::processEvents();
    const auto light = view.grab().toImage();
    EXPECT_NE(before, light);
    app.theme.themeName = "Choppa";
    QApplication::processEvents();
    const auto after = view.grab().toImage();
    EXPECT_EQ(before, after);
}

TEST(ChoppaNavigation, LoginCategoriesPreserveAllProviders)
{
    MockApplication app;
    LoginDialog dialog(nullptr);
    auto *tabs = dialog.findChild<QTabWidget *>();
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->count(), 3);
    EXPECT_TRUE(tabs->tabBar()->isHidden());
    auto *group = dialog.findChild<QButtonGroup *>();
    ASSERT_NE(group, nullptr);
    for (int index : {1, 2, 0})
    {
        ASSERT_NE(group->button(index), nullptr);
        group->button(index)->click();
        EXPECT_EQ(tabs->currentIndex(), index);
        EXPECT_TRUE(group->button(index)->isChecked());
        dialog.show();
        QApplication::processEvents();
        auto *page = tabs->currentWidget();
        for (auto *button : page->findChildren<QPushButton *>())
        {
            if (button->isVisible())
            {
                const auto bottom =
                    button->mapTo(page, button->rect().bottomRight());
                EXPECT_LT(bottom.y(), page->height());
                EXPECT_LT(bottom.x(), page->width());
            }
        }
        const auto captureDir = qEnvironmentVariable("CHOPPA_DIALOG_CAPTURES");
        if (!captureDir.isEmpty())
        {
            EXPECT_TRUE(dialog.grab().save(captureDir + "/login-" +
                                           QString::number(index) + ".png"));
        }
    }
    EXPECT_TRUE(dialog.windowFlags().testFlag(Qt::FramelessWindowHint));
}

TEST(ChoppaNavigation, SettingsSectionsFitAndRemainReachable)
{
    auto view = std::unique_ptr<GeneralPageView>(
        GeneralPageView::withNavigation(nullptr));
    view->resize(640, 300);
    view->addTitle("Interface");
    view->addDropdown("Theme", {"Choppa", "Light"});
    for (int i = 0; i < 20; ++i)
        view->addWidget(new QLabel("An ordinary setting"));
    auto *last = view->addTitle("Advanced");
    view->addWidget(new QLabel("Last setting"));
    view->show();
    QApplication::processEvents();
    auto *scroll = view->findChild<QScrollArea *>();
    auto *picker = view->findChild<QComboBox *>();
    ASSERT_NE(scroll, nullptr);
    ASSERT_NE(picker, nullptr);
    EXPECT_EQ(scroll->horizontalScrollBar()->maximum(), 0);
    picker->setCurrentIndex(1);
    Q_EMIT picker->activated(1);
    EXPECT_GT(scroll->verticalScrollBar()->value(), 0);
    EXPECT_LE(last->y() - scroll->verticalScrollBar()->value(),
              scroll->viewport()->height());
    EXPECT_FALSE(view->filterElements("nothing-matches-this"));
    EXPECT_FALSE(picker->isEnabled());
    EXPECT_TRUE(view->filterElements(""));
    EXPECT_TRUE(picker->isEnabled());
}

TEST(ChoppaModeration, PresetsDispatchCorrectCommands)
{
    MockApplication app;
    QMenu twitch;
    QString command;
    QString argument;
    auto record = [&](const QString &verb, const QString &args) {
        command = verb;
        argument = args;
    };
    appendChoppaModerationActions(&twitch, false, true, record);
    auto *timeouts = twitch.actions().at(0)->menu();
    ASSERT_NE(timeouts, nullptr);
    ASSERT_EQ(timeouts->actions().size(), 8);
    timeouts->actions().first()->trigger();
    EXPECT_EQ(command, "/timeout");
    EXPECT_EQ(argument, "30");
    timeouts->actions().last()->trigger();
    EXPECT_EQ(argument, "604800");
    twitch.actions().at(1)->menu()->actions().at(1)->trigger();
    EXPECT_EQ(command, "/ban");
    EXPECT_EQ(argument, "Spam");
    twitch.actions().at(2)->trigger();
    EXPECT_EQ(command, "/unban");
    EXPECT_TRUE(argument.isEmpty());
    twitch.actions().at(3)->menu()->actions().last()->trigger();
    EXPECT_EQ(command, "/unrestrict");
    QMenu kick;
    appendChoppaModerationActions(&kick, true, false, record);
    ASSERT_EQ(kick.actions().size(), 3);
    kick.actions().first()->menu()->actions().first()->trigger();
    EXPECT_EQ(command, "/timeout");
    EXPECT_EQ(argument, "60");
}

TEST(ChoppaNavigation, UserCardPersistsAndWindowControlsWork)
{
    MockApplication app;
    Split split(nullptr);
    QPointer<UserInfoPopup> card = new UserInfoPopup(true, &split);
    card->show();
    QApplication::processEvents();

    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(card, &deactivate);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    ASSERT_FALSE(card.isNull());
    ASSERT_EQ(card->findChildren<QWidget *>("choppaTitlebar").size(), 1);
    const auto capture = qEnvironmentVariable("CHOPPA_MEDIA_DIRECTORY");
    if (!capture.isEmpty())
        EXPECT_TRUE(card->grab().save(capture + "/usercard.png"));
    QAbstractButton *minimize = nullptr;
    QAbstractButton *maximize = nullptr;
    QAbstractButton *close = nullptr;
    for (auto *button : card->findChildren<QAbstractButton *>())
    {
        if (button->accessibleName() == "Minimize")
            minimize = button;
        if (button->accessibleName() == "Maximize / restore")
            maximize = button;
        if (button->accessibleName() == "Close")
            close = button;
    }
    ASSERT_NE(minimize, nullptr);
    ASSERT_NE(maximize, nullptr);
    ASSERT_NE(close, nullptr);
    minimize->setAttribute(Qt::WA_UnderMouse, false);
    minimize->clearFocus();
    const auto unfocused = minimize->grab().toImage();
    minimize->setFocus(Qt::ActiveWindowFocusReason);
    EXPECT_EQ(unfocused, minimize->grab().toImage());
    auto *ban = card->findChild<QPushButton *>("ban");
    auto *unban = card->findChild<QPushButton *>("unban");
    ASSERT_NE(ban, nullptr);
    ASSERT_NE(unban, nullptr);
    for (auto *button : {ban, unban})
    {
        auto render = [button](bool hover) {
            QImage image(button->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            QStyleOptionButton option;
            option.initFrom(button);
            option.text = button->text();
            option.state.setFlag(QStyle::State_MouseOver, hover);
            button->style()->drawControl(QStyle::CE_PushButton, &option,
                                         &painter, button);
            return image;
        };
        EXPECT_NE(render(false), render(true));
        EXPECT_FALSE(button->autoDefault());
    }
    unban->clearFocus();
    const auto baseColor = unban->grab().toImage().pixelColor(5, 5);
    unban->setFocus(Qt::OtherFocusReason);
    EXPECT_EQ(baseColor, unban->grab().toImage().pixelColor(5, 5));
    maximize->click();
    EXPECT_TRUE(card->isMaximized());
    maximize->click();
    EXPECT_FALSE(card->isMaximized());
    minimize->click();
    EXPECT_TRUE(card->isMinimized());
    card->showNormal();
    close->click();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(card.isNull());
}

TEST(ChoppaNavigation, EmoteWindowClosesAndReopens)
{
    MockApplication app;
    QWidget host;
    host.resize(780, 510);
    host.show();
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        QPointer<EmotePopup> popup = new EmotePopup(&host);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->show();
        QApplication::processEvents();
        EXPECT_TRUE(popup->isWindow());
        QAbstractButton *close = nullptr;
        for (auto *button : popup->findChildren<QAbstractButton *>())
            if (button->accessibleName() == "Close")
                close = button;
        ASSERT_NE(close, nullptr);
        close->click();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        EXPECT_TRUE(popup.isNull());
    }
}

TEST(ChoppaNavigation, ContextMenuStaysInsideHost)
{
    MockApplication app;
    QWidget host;
    host.setGeometry(50, 50, 780, 510);
    host.show();
    QMenu menu;
    menu.addAction("Mention user");
    appendChoppaModerationActions(&menu, false, true,
                                  [](const QString &, const QString &) {});
    ContainedMenu::install(&menu, &host);
    menu.popup(host.mapToGlobal(host.rect().bottomRight()));
    QApplication::processEvents();
    const QRect bounds(host.mapToGlobal(QPoint()), host.size());
    EXPECT_TRUE(bounds.contains(menu.geometry()));
    auto *submenu = menu.actions().at(1)->menu();
    ASSERT_NE(submenu, nullptr);
    submenu->popup(bounds.bottomRight());
    QApplication::processEvents();
    EXPECT_TRUE(bounds.contains(submenu->geometry()));
    submenu->close();
    menu.close();
}

namespace {
class ChatterListTest : public ::testing::Test
{
public:
    MockApplication app;
    testing::NiceMock<mock::Helix> helix;
    std::shared_ptr<TwitchChannel> channel;
    void SetUp() override
    {
        initializeHelix(&helix);
        channel = std::make_shared<TwitchChannel>("example");
        app.twitch.mockChannels.emplace("example", channel);
        std::unique_ptr<Communi::IrcMessage> room(Communi::IrcMessage::fromData(
            "@room-id=42 :tmi.twitch.tv ROOMSTATE #example", nullptr));
        std::unique_ptr<Communi::IrcMessage> state(
            Communi::IrcMessage::fromData(
                "@mod=1;badges=moderator/1 :tmi.twitch.tv USERSTATE #example",
                nullptr));
        IrcMessageHandler::instance().handleRoomStateMessage(room.get());
        IrcMessageHandler::instance().handleUserStateMessage(state.get());
    }
    void TearDown() override
    {
        static testing::NiceMock<mock::Helix> idleHelix;
        initializeHelix(&idleHelix);
    }
};

QPushButton *findButton(QWidget *widget, const QString &text)
{
    for (auto *button : widget->findChildren<QPushButton *>())
        if (button->text() == text)
            return button;
    return nullptr;
}
}  // namespace

TEST_F(ChatterListTest, LoadingFailureRetryAndSearchKeepOneView)
{
    ResultCallback<HelixChatters> success;
    std::function<void(HelixGetChattersError, QString)> failure;
    CancellationToken token;
    EXPECT_CALL(helix, getChatters)
        .Times(3)
        .WillRepeatedly([&](auto, auto, auto, auto ok, auto fail, auto cancel) {
            success = ok;
            failure = fail;
            token = cancel;
        });
    QWidget host;
    QPointer<ChatterListWidget> view =
        new ChatterListWidget(channel.get(), &host);
    view->show();
    QApplication::processEvents();
    auto *status = view->findChild<QLabel *>("chatterStatus");
    auto *retry = view->findChild<QPushButton *>("chatterRetry");
    auto *list = view->findChild<QListView *>("chatterList");
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(view->findChildren<QListView *>().size(), 1);
    EXPECT_FALSE(retry->isEnabled());
    retry->click();
    failure(HelixGetChattersError::Unknown, "internal transport details");
    EXPECT_TRUE(status->text().contains("Failed to load"));
    EXPECT_TRUE(retry->isEnabled());
    retry->click();
    HelixChatters users;
    users.chatters = {"example", "alice", "bob"};
    users.total = 3;
    success(users);
    const auto rows = list->model()->rowCount();
    EXPECT_EQ(rows, 5);
    EXPECT_EQ(list->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    auto *search = view->findChild<QLineEdit *>();
    search->setText("ALICE");
    {
        QEventLoop loop;
        QTimer::singleShot(120, &loop, &QEventLoop::quit);
        loop.exec();
    }
    EXPECT_EQ(list->model()->rowCount(), 1);
    EXPECT_EQ(list->model()->index(0, 0).data(Qt::UserRole + 1).toString(),
              "alice");
    search->setText("no-such-user");
    {
        QEventLoop loop;
        QTimer::singleShot(120, &loop, &QEventLoop::quit);
        loop.exec();
    }
    EXPECT_EQ(view->findChild<QLabel *>("chatterEmpty")->text(),
              "No users found.");
    search->clear();
    {
        QEventLoop loop;
        QTimer::singleShot(120, &loop, &QEventLoop::quit);
        loop.exec();
    }
    retry->setEnabled(true);  // Simulate expiry of the manual refresh cooldown.
    retry->click();
    failure(HelixGetChattersError::Ratelimited, {});
    EXPECT_EQ(list->model()->rowCount(), rows);
    EXPECT_TRUE(status->text().contains("Previously loaded"));
    EXPECT_FALSE(retry->isEnabled());
    const auto captures = qEnvironmentVariable("CHOPPA_MEDIA_DIRECTORY");
    if (!captures.isEmpty())
        EXPECT_TRUE(view->grab().save(captures + "/chatters-rate-limited.png"));
    view->close();
    EXPECT_TRUE(token.isCancelled());
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(view.isNull());
    success(users);  // Late responses cannot touch a deleted view.
}

TEST_F(ChatterListTest, EmptyAndDestroyedChannelAreSafe)
{
    ResultCallback<HelixChatters> success;
    EXPECT_CALL(helix, getChatters)
        .WillOnce([&](auto, auto, auto, auto ok, auto, auto) {
            success = ok;
        });
    QWidget host;
    auto *view = new ChatterListWidget(channel.get(), &host);
    view->show();
    channel.reset();
    success(HelixChatters{});
    EXPECT_EQ(view->findChild<QLabel *>("chatterEmpty")->text(),
              "No users are currently in this chat.");
}

TEST_F(ChatterListTest, LargeListSearchReusesModel)
{
    ResultCallback<HelixChatters> success;
    EXPECT_CALL(helix, getChatters)
        .WillOnce([&](auto, auto, auto, auto ok, auto, auto) {
            success = ok;
        });
    QWidget host;
    auto *view = new ChatterListWidget(channel.get(), &host);
    view->show();
    HelixChatters users;
    for (int i = 0; i < 50000; ++i)
        users.chatters.insert(QString("user_%1").arg(i));
    users.total = 50000;
    QElapsedTimer timer;
    timer.start();
    success(users);
    const auto loadMs = timer.elapsed();
    auto *proxy = qobject_cast<QSortFilterProxyModel *>(
        view->findChild<QListView *>()->model());
    ASSERT_NE(proxy, nullptr);
    auto *source = proxy->sourceModel();
    const auto rowCount = source->rowCount();
    timer.restart();
    proxy->setFilterFixedString("user_49999");
    EXPECT_EQ(proxy->rowCount(), 1);
    EXPECT_EQ(proxy->sourceModel(), source);
    EXPECT_EQ(source->rowCount(), rowCount);
    qInfo() << "50k chatters: populate ms" << loadMs << "filter ms"
            << timer.elapsed();
}

TEST(ChoppaNavigation, SettingsDoNotDiscardOrClaimFailedSaves)
{
    MockApplication app;
    QWidget host;
    SettingsDialog::showDialog(&host);
    QApplication::processEvents();
    auto *dialog = dynamic_cast<SettingsDialog *>(
        host.findChild<QWidget *>("SettingsDialog"));
    ASSERT_NE(dialog, nullptr);
    const bool original = app.settings.showTimestamps.getValue();
    app.settings.showTimestamps = !original;
    EXPECT_TRUE(app.settings.hasSnapshotChanges());
    EXPECT_FALSE(dialog->close());
    auto *discard = findButton(dialog, "Discard changes");
    ASSERT_NE(discard, nullptr);
    EXPECT_TRUE(discard->isVisible());
    discard->click();
    EXPECT_EQ(app.settings.showTimestamps.getValue(), original);
    EXPECT_FALSE(dialog->isVisible());
    SettingsDialog::showDialog(&host);
    app.settings.showTimestamps = !original;
    auto *save = findButton(dialog, "Save changes");
    ASSERT_NE(save, nullptr);
    auto manager = pajlada::Settings::SettingManager::getInstance();
    // Saving into a missing parent directory must fail without closing the
    // dialog or accepting the in-memory state as persisted.
    manager->setPath((app.settingsDir.path() + "/missing/subdir/settings.json")
                         .toStdWString());
    const auto restorePath = qScopeGuard([&] {
        manager->setPath(
            (app.settingsDir.path() + "/settings.json").toStdWString());
    });
    save->click();
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_TRUE(dialog->findChild<QLabel *>("settingsSaveStatus")
                    ->text()
                    .contains("could not be saved"));
    EXPECT_TRUE(app.settings.hasSnapshotChanges());
    manager->setPath(
        (app.settingsDir.path() + "/settings.json").toStdWString());
    save->click();
    EXPECT_EQ(dialog->findChild<QLabel *>("settingsSaveStatus")->text(),
              "Saved");
    EXPECT_FALSE(app.settings.hasSnapshotChanges());
    EXPECT_TRUE(dialog->close());
}

TEST(ChoppaNavigation, PopupsFitSmallHostAndReopen)
{
    MockApplication app;
    QWidget host;
    host.setGeometry(20, 20, 480, 360);
    auto *input = new QWidget(&host);
    input->setGeometry(8, 310, 460, 36);
    host.show();
    for (int i = 0; i < 3; ++i)
    {
        EmotePopup popup(input);
        popup.show();
        QApplication::processEvents();
        EXPECT_TRUE(popupAvailableRect(&host).contains(popup.geometry()))
            << "Area " << popupAvailableRect(&host).x() << ','
            << popupAvailableRect(&host).y() << ','
            << popupAvailableRect(&host).width() << ','
            << popupAvailableRect(&host).height() << " popup "
            << popup.geometry().x() << ',' << popup.geometry().y() << ','
            << popup.geometry().width() << ',' << popup.geometry().height();
        const auto Captures = qEnvironmentVariable("CHOPPA_MEDIA_DIRECTORY");
        if (!Captures.isEmpty())
            EXPECT_TRUE(popup.grab().save(Captures + "/emotes.png"));
        EXPECT_EQ(popup.findChildren<QWidget *>("choppaTitlebar").size(), 1);
        popup.close();
        EXPECT_FALSE(popup.isVisible());
    }
}

TEST_F(ChatterListTest, UserCardIgnoresPreviousUserResponses)
{
    ResultCallback<HelixUser> oldSuccess;
    HelixFailureCallback newFailure;
    EXPECT_CALL(helix, getUserById).WillOnce([&](auto, auto ok, auto) {
        oldSuccess = ok;
    });
    EXPECT_CALL(helix, getUserByName).WillOnce([&](auto name, auto, auto fail) {
        EXPECT_EQ(name, "bob");
        newFailure = fail;
    });
    Split split(nullptr);
    UserInfoPopup card(false, &split);
    card.setData("id:123", channel);
    card.setData("bob", channel);
    const auto title = card.windowTitle();
    HelixUser previous(QJsonObject{
        {"id", "123"}, {"login", "alice"}, {"display_name", "Alice"}});
    oldSuccess(previous);
    EXPECT_EQ(card.windowTitle(), title);
    EXPECT_TRUE(title.contains("bob"));
    newFailure();
    EXPECT_EQ(card.windowTitle(), title);
}

TEST(ChoppaNavigation, SecondaryPopupsHaveOneContentLayout)
{
    MockApplication app;
    QWidget host;
    SearchPopup search(&host);
    DebugPopup debug;
    for (auto *popup : {static_cast<BaseWindow *>(&search),
                        static_cast<BaseWindow *>(&debug)})
    {
        popup->show();
        QApplication::processEvents();
        auto *content = popup->getLayoutContainer();
        ASSERT_NE(content->layout(), nullptr);
        EXPECT_EQ(content->layout()->parentWidget(), content);
        EXPECT_EQ(popup->findChildren<QWidget *>("choppaTitlebar").size(), 1);
        EXPECT_TRUE(content->isVisible());
        popup->close();
    }
}
