// SPDX-License-Identifier: MIT
#include "util/ChoppaChannelImport.hpp"

#include <gtest/gtest.h>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {
QJsonObject tab(const QString &name)
{
    return {{"title", name},
            {"splits2", QJsonObject{{"type", "split"},
                                    {"data", QJsonObject{{"type", "twitch"},
                                                         {"name", name}}}}}};
}
QByteArray layout(QJsonArray tabs)
{
    return QJsonDocument(QJsonObject{{"windows", QJsonArray{QJsonObject{
                                                     {"type", "main"},
                                                     {"tabs", tabs},
                                                     {"width", 780},
                                                     {"height", 510}}}}})
        .toJson();
}
void write(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(bytes), bytes.size());
}
QByteArray read(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
}  // namespace

TEST(ChoppaChannelImport, MergesProfilesAndRetainsExistingLayoutAndBackup)
{
    QTemporaryDir dir;
    const auto destination = dir.filePath("current.json");
    const auto source = dir.filePath("zekerino.json");
    const auto old = layout({tab("existing")});
    const auto legacy = layout({tab("EXISTING"), tab("new-channel")});
    write(destination, old);
    write(source, legacy);
    EXPECT_EQ(chatterino::importChoppaChannels(destination, {source, source}),
              1);
    EXPECT_EQ(read(source), legacy);
    EXPECT_EQ(read(destination + ".before-choppa-import"), old);
    const auto windows = QJsonDocument::fromJson(read(destination))
                             .object()["windows"]
                             .toArray();
    EXPECT_EQ(windows[0].toObject()["tabs"].toArray().size(), 2);
    EXPECT_EQ(chatterino::importChoppaChannels(destination, {source}), 0);
}

TEST(ChoppaChannelImport, RemovedChannelsAreNotReimportedAfterRestart)
{
    QTemporaryDir dir;
    const auto destination = dir.filePath("current.json");
    const auto source = dir.filePath("chatterino.json");
    write(source, layout({tab("channel")}));
    ASSERT_EQ(chatterino::importChoppaChannels(destination, {source}), 1);
    const auto empty = layout({});
    write(destination, empty);
    EXPECT_EQ(chatterino::importChoppaChannels(destination, {source}), 0);
    EXPECT_EQ(read(destination), empty);
}

TEST(ChoppaChannelImport, InvalidDestinationAndSourceRemainUntouched)
{
    QTemporaryDir dir;
    const auto destination = dir.filePath("current.json");
    const auto source = dir.filePath("chatterino.json");
    write(source, layout({tab("channel")}));
    write(destination, "broken json");
    EXPECT_EQ(chatterino::importChoppaChannels(destination, {source}), 0);
    EXPECT_EQ(read(destination), "broken json");
    const auto empty = layout({});
    write(destination, empty);
    write(source, "broken json");
    EXPECT_EQ(chatterino::importChoppaChannels(destination, {source}), 0);
    EXPECT_EQ(read(destination), empty);
}

TEST(ChoppaChannelImport,
     PreservesSplitGroupsAndDropsUnresolvableForeignFilters)
{
    QTemporaryDir dir;
    const auto destination = dir.filePath("current.json");
    const auto source = dir.filePath("zekerino.json");
    auto first = tab("one")["splits2"].toObject();
    first.insert("filters", QJsonArray{"unknown-filter-id"});
    QJsonObject grouped{
        {"title", "Together"},
        {"splits2",
         QJsonObject{{"type", "horizontal"},
                     {"items", QJsonArray{first, tab("two")["splits2"]}}}}};
    write(source, layout({grouped}));
    ASSERT_EQ(chatterino::importChoppaChannels(destination, {source}), 1);
    const auto windows = QJsonDocument::fromJson(read(destination))
                             .object()["windows"]
                             .toArray();
    const auto result = windows[0].toObject()["tabs"].toArray()[0].toObject();
    EXPECT_EQ(result["title"].toString(), "Together");
    const auto children = result["splits2"].toObject()["items"].toArray();
    ASSERT_EQ(children.size(), 2);
    EXPECT_FALSE(children[0].toObject().contains("filters"));
}
