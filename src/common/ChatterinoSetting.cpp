// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "common/ChatterinoSetting.hpp"

#include "singletons/Settings.hpp"

namespace chatterino {

void _registerSetting(std::weak_ptr<pajlada::Settings::SettingData> setting,
                      std::weak_ptr<bool> lifetime,
                      SettingSnapshotFactory makeSnapshot)
{
    _actuallyRegisterSetting(std::move(setting), std::move(lifetime),
                             std::move(makeSnapshot));
}

}  // namespace chatterino
