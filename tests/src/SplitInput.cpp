// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/splits/SplitInput.hpp"

#include "common/Channel.hpp"
#include "common/Literals.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/Command.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "controllers/spellcheck/SpellChecker.hpp"
#include "messages/MessageBuilder.hpp"
#include "mocks/BaseApplication.hpp"
#include "mocks/EmoteController.hpp"
#include "mocks/UserData.hpp"
#include "mocks/TwitchIrcServer.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/CrashHandler.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "Test.hpp"
#include "widgets/ChoppaTitlebar.hpp"
#include "widgets/dialogs/LoginDialog.hpp"
#include "widgets/dialogs/UserInfoPopup.hpp"
#include "widgets/dialogs/SettingsDialog.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/helper/ChoppaModerationActions.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QAbstractButton>
#include <QDebug>
#include <QImage>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QTabBar>
#include <QVBoxLayout>

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

    IUserDataController *getUserData() override { return &this->userData; }
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
            capturedSettings = settings->grab().save(directory + "/settings.png");
            settings->close();
        }
    }
    EXPECT_TRUE(capturedSettings);
    QMenu moderation;
    moderation.setStyleSheet(
        "QMenu { background: #151515; color: #eeeeee; border: none; "
        "padding: 8px; font: 12px 'Outfit'; } QMenu::item { padding: 8px "
        "22px; border-radius: 5px; } QMenu::item:selected { background: "
        "#303030; } QMenu::separator { height: 1px; background: #292929; "
        "margin: 5px 8px; }");
    moderation.addSection("Moderation in #choppa_demo");
    appendChoppaModerationActions(&moderation, false, true,
        [](const QString &, const QString &) { ADD_FAILURE() << "Capture must not execute moderation"; });
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
        if (button->accessibleName() == "Minimize") minimize = button;
        if (button->accessibleName() == "Maximize / restore") maximize = button;
        if (button->accessibleName() == "Close") close = button;
    }
    ASSERT_NE(minimize, nullptr);
    ASSERT_NE(maximize, nullptr);
    ASSERT_NE(close, nullptr);
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
