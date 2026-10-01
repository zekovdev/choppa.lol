// SPDX-FileCopyrightText: 2017 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/Settings.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "controllers/filters/FilterRecord.hpp"
#include "controllers/highlights/HighlightBadge.hpp"
#include "controllers/highlights/HighlightBlacklistUser.hpp"
#include "controllers/highlights/HighlightPhrase.hpp"
#include "controllers/ignores/IgnorePhrase.hpp"
#include "controllers/moderationactions/ModerationAction.hpp"
#include "controllers/nicknames/Nickname.hpp"
#include "debug/Benchmark.hpp"
#include "pajlada/settings/signalargs.hpp"
#include "util/Backup.hpp"
#include "util/WindowsHelper.hpp"

#include <pajlada/signals/scoped-connection.hpp>

#include <unordered_set>

namespace {

using namespace chatterino;
using namespace Qt::Literals;

template <typename T>
void initializeSignalVector(pajlada::Signals::SignalHolder &signalHolder,
                            ChatterinoSetting<std::vector<T>> &setting,
                            SignalVector<T> &vec)
{
    // Fill the SignalVector up with initial values
    for (auto &&item : setting.getValue())
    {
        vec.append(item);
    }

    // Set up a signal to
    signalHolder.managedConnect(vec.delayedItemsChanged, [&] {
        setting.setValue(vec.raw());
    });
}

}  // namespace

namespace chatterino {

struct RegisteredSetting {
    std::weak_ptr<pajlada::Settings::SettingData> setting;
    std::weak_ptr<bool> lifetime;
    SettingSnapshotFactory makeSnapshot;
};

std::vector<RegisteredSetting> registeredSettings;

void _actuallyRegisterSetting(
    std::weak_ptr<pajlada::Settings::SettingData> setting,
    std::weak_ptr<bool> lifetime, SettingSnapshotFactory makeSnapshot)
{
    registeredSettings.push_back(
        {std::move(setting), std::move(lifetime), std::move(makeSnapshot)});
}

bool Settings::isHighlightedUser(const QString &username)
{
    auto items = this->highlightedUsers.readOnly();

    for (const auto &highlightedUser : *items)
    {
        if (highlightedUser.isMatch(username))
        {
            return true;
        }
    }

    return false;
}

bool Settings::isBlacklistedUser(const QString &username)
{
    auto items = this->blacklistedUsers.readOnly();

    for (const auto &blacklistedUser : *items)
    {
        if (blacklistedUser.isMatch(username))
        {
            return true;
        }
    }

    return false;
}

bool Settings::isMutedChannel(const QString &channelName)
{
    auto items = this->mutedChannels.readOnly();

    for (const auto &channel : *items)
    {
        if (channelName.toLower() == channel.toLower())
        {
            return true;
        }
    }
    return false;
}

std::optional<QString> Settings::matchNickname(const QString &usernameText)
{
    auto nicknames = this->nicknames.readOnly();

    for (const auto &nickname : *nicknames)
    {
        if (auto nicknameText = nickname.match(usernameText))
        {
            return nicknameText;
        }
    }

    return std::nullopt;
}

void Settings::mute(const QString &channelName)
{
    if (!this->isMutedChannel(channelName))
    {
        this->mutedChannels.append(channelName);
    }
}

void Settings::unmute(const QString &channelName)
{
    for (std::vector<int>::size_type i = 0;
         i != this->mutedChannels.raw().size(); i++)
    {
        if (this->mutedChannels.raw()[i].toLower() == channelName.toLower())
        {
            this->mutedChannels.removeAt(i);
            i--;
        }
    }
}

bool Settings::toggleMutedChannel(const QString &channelName)
{
    if (this->isMutedChannel(channelName))
    {
        this->unmute(channelName);
        return false;
    }
    else
    {
        this->mutedChannels.append(channelName);
        return true;
    }
}

Settings *Settings::instance_ = nullptr;

Settings::Settings(const Args &args, const QString &settingsDirectory,
                   const SettingsArgs &settingsArgs)
    : prevInstance_(Settings::instance_)
    , disableSaving(args.dontSaveSettings)
{
    QString settingsPath = settingsDirectory + "/settings.json";

    // get global instance of the settings library
    auto settingsInstance = pajlada::Settings::SettingManager::getInstance();

    if (settingsArgs.isTest)
    {
        settingsInstance->load(qPrintable(settingsPath));
    }
    else
    {
        backup::loadWithBackups(
            backup::FileData{
                .fileName = u"settings.json"_s,
                .directory = settingsDirectory,
                .fileKind = u"Settings"_s,
                .fileDescription =
                    u"This file contains the main application settings such as accounts and hotkeys."_s,
            },
            [&]() -> ExpectedStr<void> {
                using LoadError = pajlada::Settings::SettingManager::LoadError;
                auto err = settingsInstance->load(qPrintable(settingsPath));
                switch (err)
                {
                    case LoadError::NoError:
                        return {};  // ok
                    case LoadError::CannotOpenFile:
                        return makeUnexpected(u"Failed to open '" %
                                              settingsPath % '\'');
                    case LoadError::FileHandleError:
                        return makeUnexpected("File handle error");
                    case LoadError::FileReadError:
                        return makeUnexpected("Failed to read file");
                    case LoadError::FileSeekError:
                        return makeUnexpected("Failed to seek in file");
                    case LoadError::JSONParseError:
                        return makeUnexpected("File contained malformed JSON");
                }
                assert(false);
                return makeUnexpected("Unknown error");
            });
    }

    settingsInstance->setBackupEnabled(true);
    settingsInstance->setBackupSlots(9);
    settingsInstance->saveMethod = static_cast<
        pajlada::Settings::SettingManager::SaveMethod>(
        static_cast<uint64_t>(
            pajlada::Settings::SettingManager::SaveMethod::SaveManually) |
        static_cast<uint64_t>(
            pajlada::Settings::SettingManager::SaveMethod::OnlySaveIfChanged));

    initializeSignalVector(this->signalHolder, this->highlightedMessagesSetting,
                           this->highlightedMessages);
    initializeSignalVector(this->signalHolder, this->highlightedUsersSetting,
                           this->highlightedUsers);
    initializeSignalVector(this->signalHolder, this->highlightedBadgesSetting,
                           this->highlightedBadges);
    initializeSignalVector(this->signalHolder, this->blacklistedUsersSetting,
                           this->blacklistedUsers);
    initializeSignalVector(this->signalHolder, this->ignoredMessagesSetting,
                           this->ignoredMessages);
    initializeSignalVector(this->signalHolder, this->mutedChannelsSetting,
                           this->mutedChannels);
    initializeSignalVector(this->signalHolder, this->filterRecordsSetting,
                           this->filterRecords);
    initializeSignalVector(this->signalHolder, this->nicknamesSetting,
                           this->nicknames);
    initializeSignalVector(this->signalHolder, this->moderationActionsSetting,
                           this->moderationActions);
    initializeSignalVector(this->signalHolder, this->loggedChannelsSetting,
                           this->loggedChannels);

    instance_ = this;

#ifdef USEWINSDK
    this->autorun = isRegisteredForStartup();
    this->autorun.connect(
        [](bool autorun) {
            setRegisteredForStartup(autorun);
        },
        false);
#endif

    // migration for `/emotes/showUnlistedEmotes` -> `/emotes/showUnlistedSevenTVEmotes`
    if (this->showUnlistedEmotesDontUse && !this->showUnlistedSevenTVEmotes)
    {
        this->showUnlistedSevenTVEmotes.setValue(true);
        // reset to default, so it doesn't appear in the config
        this->showUnlistedEmotesDontUse.remove();
    }

    if (this->chatFontFamily.getValue() == "Outfit" ||
        this->chatFontFamily.getValue() == "Satoshi Variable")
    {
        this->chatFontFamily.setValue("Satoshi");
    }
    if (this->chatFontFamily.getValue() == "Satoshi" &&
        this->chatFontWeight.getValue() != QFont::Bold)
    {
        this->chatFontWeight.setValue(QFont::Bold);
    }
}

Settings::~Settings()
{
    Settings::instance_ = this->prevInstance_;
}

pajlada::Settings::SettingManager::SaveResult Settings::requestSave() const
{
    if (this->disableSaving)
    {
        return pajlada::Settings::SettingManager::SaveResult::Skipped;
    }

    return pajlada::Settings::SettingManager::gSave();
}

void Settings::saveSnapshot()
{
    BenchmarkGuard benchmark("Settings::saveSnapshot");
    this->snapshotConnections_.clear();
    this->settingSnapshot_.clear();
    std::unordered_set<std::string> connectedPaths;
    for (const auto &registered : registeredSettings)
    {
        if (registered.lifetime.expired())
            continue;
        auto setting = registered.setting.lock();
        if (!setting)
            continue;
        this->settingSnapshot_.push_back(registered.makeSnapshot());
        if (connectedPaths.insert(setting->getPath()).second)
            this->snapshotConnections_.managedConnect(
                setting->updated, [this](const auto &, const auto &) {
                    this->snapshotChanged.invoke();
                });
    }
}

bool Settings::hasSnapshotChanges() const
{
    for (const auto &snapshot : this->settingSnapshot_)
        if (snapshot.changed && snapshot.changed())
            return true;
    return false;
}

void Settings::restoreSnapshot()
{
    BenchmarkGuard benchmark("Settings::restoreSnapshot");
    for (const auto &snapshot : this->settingSnapshot_)
        if (snapshot.restore)
            snapshot.restore();
}

void Settings::disableSave()
{
    this->disableSaving = true;
}

bool Settings::shouldSendHelixChat() const
{
    switch (this->chatSendProtocol.getEnum())
    {
        case ChatSendProtocol::Helix:
            return true;
        case ChatSendProtocol::Default:
        case ChatSendProtocol::IRC:
            return false;
        default:
            assert(false && "Invalid chat protocol value");
            return false;
    }
}

float Settings::getClampedUiScale() const
{
    return std::clamp(this->uiScale.getValue(), 0.2F, 10.F);
}

void Settings::setClampedUiScale(float value)
{
    this->uiScale.setValue(std::clamp(value, 0.2F, 10.F));
}

float Settings::getClampedOverlayScale() const
{
    return std::clamp(this->overlayScaleFactor.getValue(), 0.2F, 10.F);
}

void Settings::setClampedOverlayScale(float value)
{
    this->overlayScaleFactor.setValue(std::clamp(value, 0.2F, 10.F));
}

Settings &Settings::instance()
{
    assert(instance_ != nullptr);

    return *instance_;
}

Settings *getSettings()
{
    return &Settings::instance();
}

}  // namespace chatterino
