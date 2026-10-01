// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/SettingsDialog.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/QLogging.hpp"
#include "controllers/commands/CommandController.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "singletons/Settings.hpp"
#include "util/LayoutCreator.hpp"
#include "widgets/BaseWindow.hpp"
#include "widgets/helper/SettingsDialogTab.hpp"
#include "widgets/settingspages/AboutPage.hpp"
#include "widgets/settingspages/AccountsPage.hpp"
#include "widgets/settingspages/CommandPage.hpp"
#include "widgets/settingspages/ExternalToolsPage.hpp"
#include "widgets/settingspages/FiltersPage.hpp"
#include "widgets/settingspages/GeneralPage.hpp"
#include "widgets/settingspages/HighlightingPage.hpp"
#include "widgets/settingspages/IgnoresPage.hpp"
#include "widgets/settingspages/KeyboardSettingsPage.hpp"
#include "widgets/settingspages/ModerationPage.hpp"
#include "widgets/settingspages/NicknamesPage.hpp"
#include "widgets/settingspages/NotificationPage.hpp"
#include "widgets/settingspages/PluginsPage.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QTimer>

#include <algorithm>

namespace chatterino {

SettingsDialog::SettingsDialog(QWidget *parent)
    : BaseWindow(
          {
              BaseWindow::Flags::DisableCustomScaling,
              BaseWindow::EnableCustomFrame,
              BaseWindow::ContentChrome,
              BaseWindow::Flags::Dialog,
              BaseWindow::DisableLayoutSave,
              BaseWindow::BoundsCheckOnShow,
          },
          parent)
{
    this->setObjectName("SettingsDialog");
    this->setWindowTitle("Settings");
    // Disable the ? button in the titlebar until we decide to use it
    this->setWindowFlags(this->windowFlags() &
                         ~Qt::WindowContextHelpButtonHint);

    this->resize(860, 640);
    this->setMinimumSize(720, 480);
    this->themeChangedEvent();
    QFile styleFile(":/qss/settings.qss");
    if (!styleFile.open(QFile::ReadOnly))
    {
        assert(false && "Resources not loaded");
        qCWarning(chatterinoWidget) << "Resources not loaded";
    }
    QString stylesheet = QString::fromUtf8(styleFile.readAll());
    this->setStyleSheet(stylesheet);

    this->initUi();
    this->addTabs();
    this->overrideBackgroundColor_ = QColor("#111111");

    this->addShortcuts();
    qApp->installEventFilter(this);
    this->signalHolder_.managedConnect(getSettings()->snapshotChanged, [this] {
        this->queueStateUpdate();
    });
    this->signalHolder_.managedConnect(
        getApp()->getCommands()->items.itemInserted, [this](const auto &) {
            this->queueStateUpdate();
        });
    this->signalHolder_.managedConnect(
        getApp()->getCommands()->items.itemRemoved, [this](const auto &) {
            this->queueStateUpdate();
        });
    this->signalHolder_.managedConnect(getApp()->getHotkeys()->onItemsUpdated,
                                       [this]() {
                                           this->clearShortcuts();
                                           this->addShortcuts();
                                       });
}

void SettingsDialog::addShortcuts()
{
    this->setSearchPlaceholderText();
    HotkeyController::HotkeyMap actions{
        {"search",
         [this](std::vector<QString>) -> QString {
             this->ui_.search->setFocus();
             this->ui_.search->selectAll();
             return "";
         }},
        {"delete", nullptr},
        {"accept", nullptr},
        {"reject", nullptr},
        {"scrollPage", nullptr},
        {"openTab", nullptr},
    };

    this->shortcuts_ = getApp()->getHotkeys()->shortcutsForCategory(
        HotkeyCategory::PopupWindow, actions, this);
}
void SettingsDialog::setSearchPlaceholderText()
{
    QString searchHotkey;
    auto searchSeq = getApp()->getHotkeys()->getDisplaySequence(
        HotkeyCategory::PopupWindow, "search");
    if (!searchSeq.isEmpty())
    {
        searchHotkey =
            "(" + searchSeq.toString(QKeySequence::SequenceFormat::NativeText) +
            ")";
    }
    this->ui_.search->setPlaceholderText("Find in settings... " + searchHotkey);
}

void SettingsDialog::initUi()
{
    this->getLayoutContainer()->setObjectName("settingsSurface");
    this->getLayoutContainer()->setAttribute(Qt::WA_StyledBackground, true);
    auto outerBox = LayoutCreator<QWidget>(this->getLayoutContainer())
                        .setLayoutType<QVBoxLayout>()
                        .withoutSpacing();

    // TOP
    auto title = outerBox.emplace<PageHeader>();
    auto edit = LayoutCreator<PageHeader>(title.getElement())
                    .setLayoutType<QHBoxLayout>()
                    .withoutMargin()
                    .emplace<QLineEdit>()
                    .assign(&this->ui_.search);
    this->setSearchPlaceholderText();
    edit->setClearButtonEnabled(true);
    edit->findChild<QAbstractButton *>()->setIcon(
        QPixmap(":/buttons/clearSearch.png"));
    this->ui_.search->installEventFilter(this);

    QObject::connect(edit.getElement(), &QLineEdit::textChanged, this,
                     &SettingsDialog::filterElements);

    // Compact category grid keeps the full width available to settings.
    outerBox.emplace<QWidget>()
        .assign(&this->ui_.tabContainerContainer)
        .setLayoutType<QGridLayout>()
        .withoutMargin()
        .assign(&this->ui_.tabContainer);
    this->ui_.pageTitle = new QLabel;
    this->ui_.pageTitle->setObjectName("settingsPageTitle");
    outerBox->addWidget(this->ui_.pageTitle);
    outerBox.emplace<QStackedLayout>()
        .assign(&this->ui_.pageStack)
        .withoutMargin();
    this->ui_.emptySearch =
        new QLabel(tr("No matching settings. Try another search."));
    this->ui_.emptySearch->setAlignment(Qt::AlignCenter);
    this->ui_.pageStack->addWidget(this->ui_.emptySearch);
    outerBox->setStretch(3, 1);

    this->ui_.pageStack->setContentsMargins(0, 0, 0, 0);

    outerBox->addSpacing(8);

    // BOTTOM
    this->ui_.status = new QLabel;
    this->ui_.status->setObjectName("settingsSaveStatus");
    this->ui_.status->setWordWrap(true);
    outerBox->addWidget(this->ui_.status);
    auto buttons = outerBox.emplace<QDialogButtonBox>(Qt::Horizontal);
    this->ui_.discardButton = buttons->addButton(
        "Discard changes", QDialogButtonBox::DestructiveRole);
    this->ui_.discardButton->hide();
    connect(this->ui_.discardButton, &QPushButton::clicked, this, [this] {
        getSettings()->restoreSnapshot();
        auto &commands = getApp()->getCommands()->items;
        while (!commands.raw().empty())
            commands.removeAt(commands.raw().size() - 1);
        for (const auto &command : this->commandSnapshot_)
            commands.append(command);
        this->saveOnClose_ = true;
        this->close();
    });
    {
        this->ui_.okButton =
            buttons->addButton("Save changes", QDialogButtonBox::YesRole);
        this->ui_.cancelButton =
            buttons->addButton("Cancel", QDialogButtonBox::NoRole);
    }

    // ---- misc
    this->ui_.tabContainerContainer->setObjectName("tabWidget");
    this->ui_.pageStack->setObjectName("pages");

    QObject::connect(this->ui_.okButton, &QPushButton::clicked, this,
                     &SettingsDialog::onOkClicked);
    QObject::connect(this->ui_.cancelButton, &QPushButton::clicked, this,
                     &SettingsDialog::onCancelClicked);
}

void SettingsDialog::filterElements(const QString &text)
{
    // filter elements and hide pages
    for (auto &&tab : this->tabs_)
    {
        // filterElements returns true if anything on the page matches the search query
        tab->setVisible(tab->page()->filterElements(text) ||
                        tab->name().contains(text, Qt::CaseInsensitive));
    }

    // find next visible page
    if (this->lastSelectedByUser_ && !this->lastSelectedByUser_->isHidden())
    {
        this->selectTab(this->lastSelectedByUser_, false);
    }
    else if (!this->selectedTab_ || this->selectedTab_->isHidden())
    {
        this->ui_.pageStack->setCurrentWidget(this->ui_.emptySearch);
        this->ui_.pageTitle->setText(tr("Search results"));
        for (auto &&tab : this->tabs_)
        {
            if (!tab->isHidden())
            {
                this->selectTab(tab, false);
                break;
            }
        }
    }
}

void SettingsDialog::setElementFilter(const QString &query)
{
    this->ui_.search->setText(query);
}

bool SettingsDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object == this->ui_.search && event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        if (keyEvent == QKeySequence::DeleteStartOfWord &&
            this->ui_.search->selectionLength() > 0)
        {
            this->ui_.search->backspace();
            return true;
        }
    }
    if (auto *widget = qobject_cast<QWidget *>(object);
        widget && widget->window() == this &&
        (event->type() == QEvent::KeyRelease ||
         event->type() == QEvent::FocusOut))
        this->queueStateUpdate();
    return false;
}

void SettingsDialog::addTabs()
{
    this->ui_.tabContainer->setSpacing(4);
    this->ui_.tabContainer->setContentsMargins(0, 0, 0, 8);
    for (int column = 0; column < 7; ++column)
        this->ui_.tabContainer->setColumnStretch(column, 1);

    // Constructors are wrapped in std::function to remove some strain from first time loading.

    // clang-format off
    this->addTab([]{return new GeneralPage;},          "General",        ":/settings/about.svg", SettingsTabId::General);
    this->addTab([]{return new AccountsPage;},         "Accounts",       ":/settings/accounts.svg", SettingsTabId::Accounts);
    this->addTab([]{return new NicknamesPage;},        "Nicknames",      ":/settings/accounts.svg");
    this->addTab([]{return new CommandPage;},          "Commands",       ":/settings/commands.svg");
    this->addTab([]{return new HighlightingPage;},     "Highlights",     ":/settings/notifications.svg", SettingsTabId::Highlights);
    this->addTab([]{return new IgnoresPage;},          "Ignores",        ":/settings/ignore.svg");
    this->addTab([]{return new FiltersPage;},          "Filters",        ":/settings/filters.svg");
    this->addTab([]{return new KeyboardSettingsPage;}, "Hotkeys",        ":/settings/keybinds.svg");
    this->addTab([]{return new ModerationPage;},       "Moderation",     ":/settings/moderation.svg", SettingsTabId::Moderation);
    this->addTab([]{return new NotificationPage;},     "Live Notifications",  ":/settings/notification2.svg");
    this->addTab([]{return new ExternalToolsPage;},    "External tools", ":/settings/externaltools.svg");
#ifdef CHATTERINO_HAVE_PLUGINS
    this->addTab([]{return new PluginsPage;},          "Plugins",        ":/settings/plugins.svg");
#endif
    this->addTab([]{return new AboutPage;},            "About",          ":/settings/about.svg", SettingsTabId::About, Qt::AlignBottom);
    // clang-format on
}

void SettingsDialog::addTab(std::function<SettingsPage *()> page,
                            const QString &name, const QString &iconPath,
                            SettingsTabId id, Qt::Alignment alignment)
{
    auto *tab =
        new SettingsDialogTab(this, std::move(page), name, iconPath, id);
    tab->setFixedHeight(static_cast<int>(30 * this->dpi_));

    (void)alignment;
    const int index = static_cast<int>(this->tabs_.size());
    this->ui_.tabContainer->addWidget(tab, index / 7, index % 7);
    this->tabs_.push_back(tab);

    if (this->tabs_.size() == 1)
    {
        this->selectTab(tab);
    }
}

void SettingsDialog::selectTab(SettingsDialogTab *tab, bool byUser)
{
    // add page if it's not been added yet
    [&] {
        for (int i = 0; i < this->ui_.pageStack->count(); i++)
        {
            if (this->ui_.pageStack->itemAt(i)->widget() == tab->page())
            {
                return;
            }
        }

        this->ui_.pageStack->addWidget(tab->page());
    }();

    this->ui_.pageStack->setCurrentWidget(tab->page());
    this->ui_.pageTitle->setText(tab->name());

    if (this->selectedTab_ != nullptr)
    {
        this->selectedTab_->setSelected(false);
        this->selectedTab_->setStyleSheet("color: #999999");
    }

    tab->setSelected(true);
    tab->setStyleSheet("background: #111111; color: #ffffff;"
                       "/*border: 1px solid #555; border-right: none;*/");
    this->selectedTab_ = tab;
    if (byUser)
    {
        this->lastSelectedByUser_ = tab;
    }
}

void SettingsDialog::selectTab(SettingsTabId id)
{
    auto *t = this->tab(id);
    assert(t);
    if (!t)
    {
        return;
    }

    this->selectTab(t);
}

SettingsDialogTab *SettingsDialog::tab(SettingsTabId id)
{
    for (auto &&tab : this->tabs_)
    {
        if (tab->id() == id)
        {
            return tab;
        }
    }

    assert(false);
    return nullptr;
}

void SettingsDialog::showDialog(QWidget *parent,
                                SettingsDialogPreference preferredTab)
{
    static QPointer<SettingsDialog> instance;
    if (!instance)
        instance = new SettingsDialog(parent);
    if (!instance->isVisible())
    {
        instance->refresh();
        getSettings()->saveSnapshot();
        instance->commandSnapshot_ = getApp()->getCommands()->items.raw();
    }

    switch (preferredTab)
    {
        case SettingsDialogPreference::Accounts:
            instance->selectTab(SettingsTabId::Accounts);
            break;

        case SettingsDialogPreference::Highlights:
            instance->selectTab(SettingsTabId::Highlights);
            break;

        case SettingsDialogPreference::ModerationActions:
            if (auto *tab = instance->tab(SettingsTabId::Moderation))
            {
                instance->selectTab(tab);
                if (auto *page = dynamic_cast<ModerationPage *>(tab->page()))
                {
                    page->selectModerationActions();
                }
            }
            break;

        case SettingsDialogPreference::StreamerMode: {
            instance->selectTab(SettingsTabId::General);
        }
        break;

        case SettingsDialogPreference::About: {
            instance->selectTab(SettingsTabId::About);
        }
        break;

        default:;
    }

    instance->show();
    instance->activateWindow();
    instance->raise();
    instance->setFocus();
}

void SettingsDialog::refresh()
{
    // Updates tabs.
    for (auto *tab : this->tabs_)
    {
        if (auto *page = tab->createdPage())
            page->onShow();
    }
}

void SettingsDialog::scaleChangedEvent(float newScale)
{
    assert(newScale == 1.F &&
           "Scaling is disabled for the settings dialog - its scale should "
           "always be 1");

    for (SettingsDialogTab *tab : this->tabs_)
    {
        tab->setFixedHeight(30);
    }
}

void SettingsDialog::themeChangedEvent()
{
    BaseWindow::themeChangedEvent();

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#111"));
    this->setPalette(palette);
}

void SettingsDialog::showEvent(QShowEvent *e)
{
    this->saveOnClose_ = false;
    this->saved_ = false;
    this->saveFailed_ = false;
    this->ui_.discardButton->hide();
    this->updateState();
    this->ui_.search->setText("");
    BaseWindow::showEvent(e);
}

bool SettingsDialog::hasChanges() const
{
    const auto &commands = getApp()->getCommands()->items.raw();
    return getSettings()->hasSnapshotChanges() ||
           !std::equal(
               commands.begin(), commands.end(), this->commandSnapshot_.begin(),
               this->commandSnapshot_.end(),
               [](const Command &a, const Command &b) {
                   return a.name == b.name && a.func == b.func &&
                          a.showInMsgContextMenu == b.showInMsgContextMenu;
               });
}

bool SettingsDialog::hasInvalidInput() const
{
    for (auto *edit : this->findChildren<QLineEdit *>())
        if (edit != this->ui_.search && edit->isVisible() &&
            edit->isEnabled() && edit->validator() &&
            !edit->hasAcceptableInput())
            return true;
    return false;
}

void SettingsDialog::queueStateUpdate()
{
    if (!this->isVisible() || this->stateUpdatePending_)
        return;
    this->stateUpdatePending_ = true;
    QTimer::singleShot(0, this, [this] {
        this->stateUpdatePending_ = false;
        this->updateState();
    });
}

void SettingsDialog::updateState()
{
    const bool modified = this->hasChanges();
    const bool invalid = this->hasInvalidInput();
    this->ui_.okButton->setEnabled(!invalid &&
                                   getSettings()->isSavingEnabled());
    this->ui_.okButton->setToolTip(
        invalid ? tr("Correct the invalid value before saving.")
        : !getSettings()->isSavingEnabled()
            ? tr("Saving is disabled for this session.")
            : QString());
    this->ui_.cancelButton->setText(modified ? tr("Cancel") : tr("Close"));
    if (invalid)
        this->ui_.status->setText(
            tr("A value is invalid. Correct it before saving."));
    else if (!getSettings()->isSavingEnabled())
        this->ui_.status->setText(tr("Saving is disabled for this session."));
    else if (this->saveFailed_)
        return;
    else if (modified)
        this->ui_.status->setText(tr("Unsaved changes"));
    else
        this->ui_.status->setText(this->saved_ ? tr("Saved")
                                               : tr("No changes"));
}

void SettingsDialog::onOkClicked()
{
    if (this->hasInvalidInput() || !getSettings()->isSavingEnabled())
    {
        this->updateState();
        return;
    }
    this->ui_.status->setText(tr("Saving…"));
    this->ui_.okButton->setEnabled(false);
    using Result = pajlada::Settings::SettingManager::SaveResult;
    const auto commands = getApp()->getCommands()->save();
    const auto settings = getSettings()->requestSave();
    if (commands == Result::Failed || settings == Result::Failed)
    {
        this->saveFailed_ = true;
        this->ui_.status->setText(
            tr("Some changes could not be saved. Check disk space and folder "
               "permissions, then retry Save changes."));
        this->ui_.okButton->setEnabled(true);
        return;
    }
    this->saveFailed_ = false;
    this->saved_ = true;
    getSettings()->saveSnapshot();
    this->commandSnapshot_ = getApp()->getCommands()->items.raw();
    this->ui_.discardButton->hide();
    this->updateState();
}

void SettingsDialog::onCancelClicked()
{
    this->close();
}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
    if (!this->saveOnClose_ && this->hasChanges())
    {
        event->ignore();
        this->ui_.status->setText(tr(
            "Unsaved changes. Save them or choose Discard changes to close."));
        this->ui_.discardButton->show();
        return;
    }
    BaseWindow::closeEvent(event);
}

void SettingsDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        this->onCancelClicked();
    else
        BaseWindow::keyPressEvent(event);
}

}  // namespace chatterino
