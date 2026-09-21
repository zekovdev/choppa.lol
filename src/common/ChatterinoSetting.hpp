// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "util/QMagicEnum.hpp"

#include <pajlada/settings.hpp>
#include <QSize>
#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

namespace chatterino {

struct SettingSnapshot {
    std::function<bool()> changed;
    std::function<void()> restore;
};

using SettingSnapshotFactory = std::function<SettingSnapshot()>;

void _registerSetting(std::weak_ptr<pajlada::Settings::SettingData> setting,
                      std::weak_ptr<bool> lifetime,
                      SettingSnapshotFactory makeSnapshot);

template <typename Type>
class ChatterinoSetting : public pajlada::Settings::Setting<Type>
{
    std::shared_ptr<bool> registrationLifetime_ = std::make_shared<bool>(true);

    void registerForSnapshots()
    {
        if constexpr (std::is_same_v<Type, QSize>)
        {
            return;
        }
        else
        {
            const std::weak_ptr<bool> lifetime = this->registrationLifetime_;
            _registerSetting(this->getData(), lifetime, [this, lifetime] {
                const Type original = this->getValueCopy();
                return SettingSnapshot{[this, lifetime, original] {
                                           return !lifetime.expired() &&
                                                  this->getValue() != original;
                                       },
                                       [this, lifetime, original] {
                                           if (!lifetime.expired())
                                               this->setValue(original);
                                       }};
            });
        }
    }

public:
    ChatterinoSetting(const std::string &path)
        : pajlada::Settings::Setting<Type>(
              path, pajlada::Settings::SettingOption::CompareBeforeSet)
    {
        this->registerForSnapshots();
    }

    ChatterinoSetting(const std::string &path, const Type &defaultValue)
        : pajlada::Settings::Setting<Type>(
              path, defaultValue,
              pajlada::Settings::SettingOption::CompareBeforeSet)
    {
        this->registerForSnapshots();
    }

    template <typename T2>
    ChatterinoSetting &operator=(const T2 &newValue)
    {
        this->setValue(newValue);

        return *this;
    }

    ChatterinoSetting &operator=(Type &&newValue) noexcept
    {
        pajlada::Settings::Setting<Type>::operator=(newValue);

        return *this;
    }

    using pajlada::Settings::Setting<Type>::operator==;

    using pajlada::Settings::Setting<Type>::operator Type;
};

using BoolSetting = ChatterinoSetting<bool>;
using FloatSetting = ChatterinoSetting<float>;
using DoubleSetting = ChatterinoSetting<double>;
using IntSetting = ChatterinoSetting<int>;
using UInt64Setting = ChatterinoSetting<uint64_t>;
using StringSetting = ChatterinoSetting<std::string>;
using QStringSetting = ChatterinoSetting<QString>;
using QSizeSetting = ChatterinoSetting<QSize>;

/// Accepts any enum and saves the enum value as an integer
///
/// e.g. for enum class {Foo = 2, Bar = 6}, Foo would be saved as 2 and Bar would be saved as 6
template <typename Enum>
class EnumSetting : public ChatterinoSetting<std::underlying_type_t<Enum>>
{
    using Underlying = std::underlying_type_t<Enum>;

public:
    using ChatterinoSetting<Underlying>::ChatterinoSetting;

    EnumSetting(const std::string &path, const Enum &defaultValue)
        : ChatterinoSetting<Underlying>(path, Underlying(defaultValue))
    {
    }

    EnumSetting<Enum> &operator=(Enum newValue)
    {
        this->setValue(Underlying(newValue));

        return *this;
    }

    operator Enum()
    {
        return Enum(this->getValue());
    }

    Enum getEnum()
    {
        return Enum(this->getValue());
    }
};

/**
 * Setters in this class allow for bad values, it's only the enum-specific getters that are protected.
 * If you get a QString from this setting, it will be the raw value from the settings file.
 * Use the explicit Enum conversions or getEnum to get a typed check with a default
 **/
template <typename Enum>
class EnumStringSetting : public pajlada::Settings::Setting<QString>
{
    std::shared_ptr<bool> registrationLifetime_ = std::make_shared<bool>(true);

    void registerForSnapshots()
    {
        const std::weak_ptr<bool> lifetime = this->registrationLifetime_;
        _registerSetting(this->getData(), lifetime, [this, lifetime] {
            const QString original = this->getValueCopy();
            return SettingSnapshot{[this, lifetime, original] {
                                       return !lifetime.expired() &&
                                              this->getValue() != original;
                                   },
                                   [this, lifetime, original] {
                                       if (!lifetime.expired())
                                           this->setValue(original);
                                   }};
        });
    }

public:
    EnumStringSetting(const std::string &path, const Enum &defaultValue_)
        : pajlada::Settings::Setting<QString>(path)
        , defaultValue(defaultValue_)
    {
        this->registerForSnapshots();
    }

    template <typename T2>
    EnumStringSetting<Enum> &operator=(Enum newValue)
    {
        this->setValue(qmagicenum::enumNameString(newValue).toLower());

        return *this;
    }

    EnumStringSetting<Enum> &operator=(QString newValue)
    {
        this->setValue(newValue.toLower());

        return *this;
    }

    operator Enum()
    {
        return this->getEnum();
    }

    Enum getEnum() const
    {
        return qmagicenum::enumCast<Enum>(this->getValue(),
                                          qmagicenum::CASE_INSENSITIVE)
            .value_or(this->defaultValue);
    }

    Enum defaultValue;

    using pajlada::Settings::Setting<QString>::operator==;

    using pajlada::Settings::Setting<QString>::operator QString;
};

template <typename T>
struct IsChatterinoSettingT : std::false_type {
};
template <typename T>
struct IsChatterinoSettingT<ChatterinoSetting<T>> : std::true_type {
};
template <typename T>
struct IsChatterinoSettingT<EnumStringSetting<T>> : std::true_type {
};

template <typename T>
concept IsChatterinoSetting = IsChatterinoSettingT<T>::value;

}  // namespace chatterino
