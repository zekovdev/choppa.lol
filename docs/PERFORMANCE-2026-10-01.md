# Performance and quality update

The 1.1.0 Windows installer includes coalesced chat layouts, reusable message snapshots, faster emote completion, interruptible history filtering and fixes for widget, timer and image lifetimes. Existing channels, settings, moderation actions, themes and plugins remain compatible. Satoshi Bold and font smoothing remain enabled.

The audit covered the message and rendering pipeline, scroll handling, tabs and splits, popup ownership, settings connections, filters and highlights, completion, image caches and expiration, logging, network workers, startup and shutdown. Networking and image decoding already use worker threads. Message state, filters and QPixmap application retain their existing GUI ownership rather than introducing concurrent access to mutable state.

The table compares the original build at `2e8e2fa48107bdc77e8b43ce02a232c7a0ac86f9` with the median of three final runs on the same PC. Measurements use MSVC Release, Qt 6.8.3 and offscreen raster rendering. Four 640 x 360 chat views each retain 1,000 messages. The workload uses text, umlauts and shared static image elements. Rendering and scroll samples include all four views. Smooth scrolling is disabled in this synthetic benchmark to measure immediate layout and painting; the application still supports it.

| Workload | Before | After |
| --- | ---: | ---: |
| Add 2,000 messages, then process updates and render | 104.708 ms | 5.421 ms |
| Add messages before processing queued paints | 100.839 ms | 1.104 ms |
| Read an unchanged snapshot 10,000 times | 58.156 ms | 0.015 ms |
| Paint four views, median | 0.208 ms | 0.171 ms |
| Paint four views, 95th percentile | 0.321 ms | 0.230 ms |
| Scroll and render four views, median | 2.854 ms | 2.797 ms |
| Scroll and render four views, 95th percentile | 3.492 ms | 3.385 ms |
| Complete against 4,000 emotes, median | 3.132 ms | 1.430 ms |

The main measured improvement comes from merging message-triggered layouts into a paint update and avoiding unchanged snapshot copies. Completion calculates ranking once per candidate and sorts small ranking entries. Scrolling remains in the same timing range; these samples do not establish a significant scrolling speedup.

Image URL and pixmap caches periodically remove expired weak entries. Pixmaps are keyed by content identity rather than the address of a temporary object. Image expiration releases ownership outside its registry lock, preventing reentrant destruction from acquiring the same mutex. Offscreen message buffers are released after they leave the visible range, including when a view becomes empty.

History filtering yields between short batches, cancels superseded queries and stops when its window closes. The result count distinguishes total matches from retained results. Preparing and ordering a combined history snapshot still happens on the GUI thread; an unusually large multi-channel history or an expensive individual regular expression can take longer than a filtering batch.

Settings controls now destroy their connection owner together with the visible control. Temporary settings pages, text editors, logging channels, emoji providers, plugin controllers and GIF timers disconnect their callbacks when destroyed. The delayed layout-save timer belongs to its window manager and cannot run after shutdown. Badge relayout and image completion tolerate an unavailable window manager. Importing an upload provider clears headers and deletion links inherited from a previous provider.

Log writes use QFile buffering and flush at most once per 500 ms interval instead of once per message. Normal close and file rotation flush the buffer. An abrupt process termination can lose the most recent buffered interval. File opening and writing still occur on the GUI thread, so a slow or unavailable disk can cause delays.

Search, emote and other content-chrome windows use the same dark surface and border across activation changes. Popup placement accounts for the difference between Qt frame and client coordinates at high DPI. The search controls stay compact so the results receive the remaining height. Current captures come from the actual Qt application with fictional offline messages.

Installer shortcuts use the application's existing Windows AppUserModelID and a stable installation path. Inno Setup supports assigning this ID directly to shortcuts through its [Icons section](https://jrsoftware.org/ishelp/topic_iconssection.htm).

Validation passed 611 local tests from 71 suites with a normal process exit. These include IRC snapshots and cloning, completion, filtering, moderation, channel import, account setup, theme round trips, popup close and reopen, settings lifetime, logging and image cleanup. The 77 legacy IRC snapshots were updated only for the already-existing dynamic 7TV badge representation; all other serialized fields were compared unchanged. Seven UI checks passed at each of 100%, 125%, 150% and 200% scaling.

A sustained-load regression inserts 30,000 messages with a 1,000-message retention limit and verifies that evicted messages and cleared visible layouts release their references. Image tests exercise 20,000 transient URL entries, pixmap content changes and 20 rounds of concurrent ownership release during collection. These are bounded workload tests, not an hours-long live-stream soak test or a process-wide leak detector.

Network integration suites require the separate HTTP and WebSocket test services. They were excluded from the local run: `NetworkRequest.*`, `TwitchPubSubClient.*`, `BasicPubSub.*`, `SeventvEventAPI.*`, `BttvLiveUpdates.*`, `WebSocketPool.*`, and the HTTP/TCP/TLS plugin integration cases. Live Twitch/Kick authentication and server behavior were not benchmarked. CPU percentages, process-wide allocation counts, GPU presentation latency, total RAM and startup time were not measured, so this report makes no numerical claims about those metrics.

Raw measurements are saved in [results.json](performance/2026-10-01/results.json). The benchmark is opt-in:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
$env:CHOPPA_PERF_RESULTS = "$PWD/chat-results.json"
$env:CHOPPA_COMPLETION_RESULTS = "$PWD/completion-results.json"
& './build/bin/chatterino-test.exe' '--gtest_filter=ChoppaPerformance.*'
```

Build paths vary with the local CMake configuration. Run the benchmark without a concurrent build or another test run, and compare identical settings and workloads. One baseline run and three final runs show a clear reduction in burst and completion work; they do not guarantee identical results on other hardware or eliminate every possible source of stalls.
