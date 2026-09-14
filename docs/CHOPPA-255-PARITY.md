# Choppa / Chatterino 2.5.5 comparison

Source: https://chatterino.com/changelog#2.5.5, checked 2026-09-14.
The supplied Zekerino tree already contains the 2.5.5 and 2.5.5-beta.1
changes. This update preserves those implementations and fixes build/UI gaps.
It does not claim that every inherited feature was newly implemented here.

## Existing implementation retained

| Feature group | Implementation evidence |
| --- | --- |
| Lead moderator authority and badges | `providers/twitch/IrcMessageHandler.cpp`, `TwitchBadge.cpp`, `messages/MessageBuilder.cpp` |
| Slow-mode / timeout countdown | `widgets/splits/SplitInput.cpp`: sendWaitStatus; General settings: showSendWaitTimer |
| Unicode 17 and emoji categories | Emoji resources and `widgets/dialogs/EmotePopup.cpp`; CHANGELOG #6471/#6598 |
| Poll creation/end/cancel | `controllers/commands/builtin/twitch/Poll.cpp` |
| Prediction create/lock/cancel/complete | `controllers/commands/builtin/twitch/Prediction.cpp` |
| Suspicious users | `controllers/commands/builtin/twitch/LowTrust.cpp` |
| BetterTTV Pro badges | Inherited #6625/#6724 implementation |
| Search selected text with configured engine | `widgets/helper/ChannelView.cpp`, General settings |
| Watch-streak highlights and filters | `controllers/filters/lang/Tokenizer.cpp`, `IdentifierExpression.cpp` |
| Deleted-message length, live titles, clip options | Settings; command implementations; inherited #6491/#6572/#6669 |
| External badge filters and Markdown notes | Filter identifiers and user-notes implementation; #6709/#6490 |
| Reset watching and follower duration | Split header/channel handling; #6759/#6812 |
| Keep sound backend alive | Settings: /sound/miniaudio/keepEngineAlive |
| Lua message read/update, links, JSON, account, traceback, display-name events | `controllers/plugins/api`, plugin metadata; plugins are enabled in the build |
| Network shutdown, settings persistence, websocket retry fixes | Inherited 2.5.5 changes; see source CHANGELOG |
| Other platform, shortcut, account, badge and font fixes | Inherited 2.5.5-beta.1 / 2.5.5 changes; see source CHANGELOG |
| Build requirements | C++23; Windows Qt 6.8.3; no automatic upstream updater in this custom distribution |

Of 90 PR numbers referenced in the supplied 2.5.5 text, 86 are explicitly in
the local CHANGELOG. The remaining numbers concern Qt/Ubuntu packaging and
Enter-key/watch-streak changes. Enter normalization and watch_streak identifiers
are present in the code under differently numbered local entries. Ubuntu
packaging does not apply to this Windows artifact. Changelog matching is
provenance evidence, not a substitute for runtime testing of every API.

## Changes in this package

- Enable Hunspell in the Windows build (previously compiled out).
- Include German and US-English dictionaries and their licenses. German data
  is converted to UTF-8 for the existing checker, with unchanged words/rules.
  Select a dictionary under Settings > External tools. Spell checking stays
  opt-in; existing user settings are preserved.
- Reuse the full notebook context menu in the Choppa sidebar and top tabs:
  alphabetic sort, visibility, layout lock, close multiple tabs, duplicate,
  rename, pop out and unread marking. Both paths use the same command handlers
  and existing confirmation dialogs. The shared menu has Choppa styling.
- Add monitoring/restriction actions to the Twitch user-name moderation menu,
  with the same rights and target checks as other moderation actions.

Twitch API permissions still apply. Polls/predictions require broadcaster
permissions; this client cannot invent unsupported lead-moderator API access.
No live polls, predictions, bans or restrictions are executed for testing.
Channel-points farming remains excluded as requested.
