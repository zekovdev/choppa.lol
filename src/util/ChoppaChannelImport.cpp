// SPDX-License-Identifier: MIT
#include "util/ChoppaChannelImport.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace chatterino {
namespace {
QJsonObject readLayout(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
        return {};
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject() || !document.object().value("windows").isArray())
        return {};
    return document.object();
}

QJsonObject channelIdentity(QJsonObject node, int depth = 0)
{
    if (depth > 32)
        return {};
    const auto type = node.value("type").toString();
    if (type == "split")
    {
        auto data = node.value("data").toObject();
        if (data.value("type").toString().isEmpty() ||
            data.value("type") == "empty")
            return {};
        if (data.contains("name"))
            data.insert("name", data.value("name").toString().toCaseFolded());
        return {{"data", data}};
    }
    if (type != "horizontal" && type != "vertical")
        return {};
    QJsonArray children;
    for (const auto &child : node.value("items").toArray())
    {
        auto identity = channelIdentity(child.toObject(), depth + 1);
        if (!identity.isEmpty())
            children.append(identity);
    }
    return children.isEmpty() ? QJsonObject{} : QJsonObject{{type, children}};
}

QJsonObject withoutForeignFilters(QJsonObject node, int depth = 0)
{
    if (depth > 32)
        return {};
    node.remove("filters");
    if (node.contains("items"))
    {
        QJsonArray items;
        for (const auto &item : node.value("items").toArray())
            items.append(withoutForeignFilters(item.toObject(), depth + 1));
        node.insert("items", items);
    }
    return node;
}
}  // namespace

QStringList choppaLegacyLayoutPaths()
{
    QStringList result;
    const auto addRoot = [&](const QString &root) {
        result.append(QDir(root).filePath("Settings/window-layout.json"));
        result.append(QDir(root).filePath("window-layout.json"));
    };
    for (const auto &base :
         {QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
          qEnvironmentVariable("APPDATA"),
          qEnvironmentVariable("LOCALAPPDATA")})
    {
        if (base.isEmpty())
            continue;
        for (const auto &name : {"Chatterino2", "Chatterino", "Zekerino",
                                 "Zekerino2", "7TV Chatterino"})
            addRoot(QDir(base).filePath(name));
    }
    // Also recognize portable profiles next to or above the new installation.
    QDir nearby(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 3; ++level)
    {
        addRoot(nearby.path());
        for (const auto &name :
             {"Chatterino", "Chatterino2", "Zekerino", "Zekerino-main"})
            addRoot(nearby.filePath(name));
        if (!nearby.cdUp())
            break;
    }
    result.removeDuplicates();
    return result;
}

int importChoppaChannels(const QString &destination, const QStringList &sources)
{
    const QString marker = destination + ".choppa-imported";
    if (QFileInfo::exists(marker))
        return 0;
    auto target = readLayout(destination);
    // A malformed existing layout is left to the application's backup recovery.
    if (QFileInfo::exists(destination) && target.isEmpty())
        return 0;
    auto windows = target.value("windows").toArray();
    QSet<QByteArray> seen;
    int mainIndex = -1;
    for (int i = 0; i < windows.size(); ++i)
    {
        const auto window = windows[i].toObject();
        if (window.value("type") == "main")
            mainIndex = i;
        for (const auto &tab : window.value("tabs").toArray())
        {
            const auto identity =
                channelIdentity(tab.toObject().value("splits2").toObject());
            if (!identity.isEmpty())
                seen.insert(
                    QJsonDocument(identity).toJson(QJsonDocument::Compact));
        }
    }
    QJsonArray imported;
    for (const auto &source : sources)
    {
        if (QFileInfo(source).absoluteFilePath().compare(
                QFileInfo(destination).absoluteFilePath(),
                Qt::CaseInsensitive) == 0)
            continue;
        for (const auto &window : readLayout(source).value("windows").toArray())
            for (const auto &entry : window.toObject().value("tabs").toArray())
            {
                auto tab = entry.toObject();
                const auto identity =
                    channelIdentity(tab.value("splits2").toObject());
                if (identity.isEmpty())
                    continue;
                const auto key =
                    QJsonDocument(identity).toJson(QJsonDocument::Compact);
                if (seen.contains(key))
                    continue;
                seen.insert(key);
                tab.remove("selected");
                tab.insert("splits2", withoutForeignFilters(
                                          tab.value("splits2").toObject()));
                imported.append(tab);
            }
    }
    if (imported.isEmpty())
        return 0;
    if (mainIndex < 0)
    {
        mainIndex = windows.size();
        windows.append(
            QJsonObject{{"type", "main"}, {"width", 780}, {"height", 510}});
    }
    auto main = windows[mainIndex].toObject();
    auto tabs = main.value("tabs").toArray();
    for (const auto &tab : imported)
        tabs.append(tab);
    main.insert("tabs", tabs);
    windows[mainIndex] = main;
    target.insert("windows", windows);
    QDir().mkpath(QFileInfo(destination).absolutePath());
    const QString backup = destination + ".before-choppa-import";
    if (QFileInfo::exists(destination) && !QFileInfo::exists(backup) &&
        !QFile::copy(destination, backup))
        return 0;
    QSaveFile output(destination);
    const auto bytes = QJsonDocument(target).toJson();
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(bytes) != bytes.size() || !output.commit())
        return 0;
    QFile importedMarker(marker);
    if (importedMarker.open(QIODevice::WriteOnly))
        importedMarker.write("Channel import completed\n");
    return imported.size();
}
}  // namespace chatterino
