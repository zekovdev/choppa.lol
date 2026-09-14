// SPDX-License-Identifier: MIT
#pragma once

#include <QStringList>

namespace chatterino {
QStringList choppaLegacyLayoutPaths();
// Imports channel layouts once, retaining the destination and backing it up.
// Credentials and application settings are never part of this operation.
int importChoppaChannels(const QString &destination,
                         const QStringList &sources);
}  // namespace chatterino
