#pragma once

#include <QMenu>
#include <QCoreApplication>
#include <functional>
#include <utility>
#include <vector>

namespace chatterino {

// Shared by the live name menu and the offline feature capture.
inline void appendChoppaModerationActions(
    QMenu *menu, bool isKick, bool isTwitch,
    const std::function<void(const QString &, const QString &)> &execute)
{
    const auto tr = [](const char *text) { return QCoreApplication::translate("chatterino::ChannelView", text); };
    auto *timeouts = menu->addMenu(tr("Timeout"));
    for (const auto &[label, seconds] :
         std::vector<std::pair<QString, int>>{{tr("30 seconds"), 30},
                                              {tr("1 minute"), 60},
                                              {tr("5 minutes"), 300},
                                              {tr("10 minutes"), 600},
                                              {tr("30 minutes"), 1800},
                                              {tr("1 hour"), 3600},
                                              {tr("1 day"), 86400},
                                              {tr("1 week"), 604800}})
    {
        if (isKick && seconds < 60)
            continue;
        timeouts->addAction(label, menu, [execute, seconds] {
            execute("/timeout", QString::number(seconds));
        });
    }
    auto *ban = menu->addMenu(tr("Ban"));
    ban->addAction(tr("Ban user"), menu, [execute] {
        execute("/ban", {});
    });
    for (const QString &reason :
         {QString("Spam"), QString("Harassment"),
          QString("Repeated rule violations")})
        ban->addAction(reason, menu, [execute, reason] {
            execute("/ban", reason);
        });
    menu->addAction(tr("Unban / remove timeout"), menu, [execute] {
        execute("/unban", {});
    });
    if (isTwitch)
    {
        auto *suspicious = menu->addMenu(tr("Suspicious user"));
        suspicious->addAction(tr("Monitor messages"), menu, [execute] {
            execute("/monitor", {});
        });
        suspicious->addAction(tr("Stop monitoring"), menu, [execute] {
            execute("/unmonitor", {});
        });
        suspicious->addAction(tr("Restrict messages"), menu, [execute] {
            execute("/restrict", {});
        });
        suspicious->addAction(tr("Remove restriction"), menu,
                              [execute] {
                                  execute("/unrestrict", {});
                              });
    }
}
}


