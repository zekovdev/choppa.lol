# Choppa Chat desktop

## Theme and popup consistency revision

- Existing message layouts are invalidated on theme changes, so cached text
  colors are resolved again. A rendered Choppa/Light/Choppa round-trip test
  verifies that existing messages return to their original appearance.
- The shared titlebar suppresses the redundant Choppa window title beside the
  brand. Dialog titles remain visible.
- Chatter lists and non-frameless BasePopup windows use ContentChrome. This
  includes search, notes and color selection; completion popups remain compact.
- Chatter list callbacks check window lifetime before touching widgets. Failure
  callbacks stop the loading indicator, and search/user activation excludes
  headings and explanatory rows. Bulk inserts suppress intermediate repaints.
- 47 focused tests passed. Native visual verification was blocked by the
  Windows helper error `foreground window did not report a process id`.
  This is not a guarantee of complete UI coverage or absence of other defects.

This redesign uses the local Choppa product as its visual reference:

- `D:/choppadevelopment/app/globals.css`
- `D:/choppadevelopment/app/dashboard/components/sidebar.css`
- `D:/choppadevelopment/app/dashboard/components/Sidebar.tsx`
- The original logo and Outfit font files from `public/`.

The current tokens are `#0a0a0a` for the desktop background, `#111111` for
surfaces, `#1a1a1a` for controls, `#d6d6d6` for the accent, and an 8 px radius.
The code follows these current tokens rather than older purple/blue themes.

## Architecture

`ChoppaTitlebar` provides shared window controls for the main window, settings,
and channel selection. `BaseWindow::ContentChrome` retains Windows border
resizing and caption hit testing while replacing the old titlebar widgets.

`ChoppaShell` owns the searchable workspace navigation, account control,
streamer-mode control and workspace actions. The notebook still owns the actual
chat pages. Navigating, filtering, or rearranging workspaces does not recreate
Twitch connections or discard message history.

The sidebar observes `Notebook::navigationChanged`. Updates are coalesced in
the Qt event loop, without a polling timer. Offscreen legacy tab widgets no
longer request their own painting when external navigation is active. Live
status, unread messages, and mentions retain their existing source of truth.

## Interaction

- **Channel:** open a channel in a new workspace.
- **Split:** add a channel pane to the current workspace.
- **New workspace:** create an empty workspace.
- **Ctrl+K:** focus workspace search through the existing configurable
  `openQuickSwitcher` action. There is no competing hardcoded shortcut.
- **Enter in search:** select the first matching workspace.
- **Right-click a workspace:** rename, duplicate, or close.
- **Drag a workspace:** reorder it. The existing layout lock is respected.
- **Sidebar button:** switch between compact wrapping channel rows and the searchable sidebar.
- **Account:** use the existing account switcher and account management.

Canceling channel selection removes the temporary split and newly created
workspace. Existing profiles using the old Zekerino default themes migrate to
Choppa once. Subsequent theme choices and custom chat fonts remain respected.

## Build and verification

The existing Windows build instructions remain applicable. The local validation
build uses Qt 6.8.3, the installed Visual Studio C++ toolchain, Conan 2, and Ninja.
Configure with `-DBUILD_TESTS=ON -DCHATTERINO_UPDATER=OFF`. The stock updater is
disabled for this local custom build so it cannot replace it with an upstream
release.

The build tools and Conan cache are isolated in `build-choppa/`. Original empty
submodule directories were populated from the existing `D:/modderino` checkout;
that checkout and its running client are not the build destination.

Focused regression coverage is in `tests/src/SplitInput.cpp`, under
`ChoppaNavigation.*`. It covers page identity, selection, close behavior, title
changes and unread notifications. Existing notebook and input tests should also
be run after changes to these paths.

Do not claim a general speed improvement without measuring a comparable
workload. The implemented performance change specifically avoids rebuilding
chat pages and painting hidden tab controls for the new navigation.

The Outfit license is bundled in `resources/choppa/OFL.txt`. Original Chatterino
and Zekerino attribution remains in the About page and source licenses.

## Verified release (2026-09-14)

Release build completed successfully. The 15 focused tests across ChoppaNavigation,
NotebookTabFixture and SplitInput passed. Native Windows UI checks covered startup
with a fresh portable profile, existing chats/emotes, the settings dialog, channel
selection and cancellation, and maximizing/restoring the main window.
The packaged executable is ChoppaChat.exe and includes Qt, OpenSSL and the MSVC
runtime. A modes file containing portable selects an isolated profile next to the EXE.
Full Twitch sign-in, moderation actions, long-duration stability and comparative
performance benchmarks were not part of this verification.

## Compact revision

The main window starts at a compact 780 by 510 logical client size. The large
heading and footer were removed, navigation now wraps into up to four rows, and
the optional sidebar is 176 pixels wide. Chat input uses a single horizontal
row with centered text, counters and a 26 pixel emote button (20 pixel artwork).
Channel and settings dialogs use smaller controls. Without saved accounts,
the sidebar account button opens account management directly. The account
switcher sizes itself to the actual account count instead of reserving 280 pixels.

On first migration, channel layouts are merged from standard Chatterino and
Zekerino profile directories and nearby portable profile locations. Existing
destination layouts are backed up before atomic replacement; sources remain
untouched. Duplicate channel groups are skipped. Split groups and titles are
retained; references to filters from another profile are removed because their
definitions are not imported. Credentials are not imported. A completion marker
prevents channels deliberately closed later from returning on restart. Arbitrary
portable installations elsewhere on disk must be placed beside the new app or
their layout copied into its Settings folder before the first launch.

This revision passed 19 targeted tests, including profile merging, duplicate
handling, backup preservation, malformed files, restart behavior, split groups,
existing notebook behavior and reply inputs. Native UI verification confirmed
automatic loading of the user's 33 workspaces, compact channel rows, sidebar
switching and the centered enlarged emote control.
# Live, emotes and settings revision

The red workspace dot exclusively represents a live channel. Unread workspaces
use bold names; mentions/highlights additionally use an amber underline. Reading
a workspace clears its unread state without affecting the independent live state.
Hover text and accessible descriptions explain each active status.

Twitch live metadata uses the existing authenticated Twitch API. Sign in through
Settings > Accounts; anonymous chat alone cannot supply authenticated live data.
No channel-points earning or farming feature is included.

The emote picker uses Choppa window controls, its own category buttons and global
search, defaults to Channel, preserves animated emote rendering and insertion,
shows an empty-search explanation, and initially opens beside the chat input on
the same monitor. Saved bounds remain respected.

Settings now use a two-row category grid and full-width content. General settings
have a section picker instead of a right-hand navigation column. Search has an
explicit no-results state, and reopening clears the previous search correctly.
Save changes commits settings; Cancel, Escape and X restore the settings snapshot.
Account-management actions retain their existing behavior.

Window controls draw centered vector glyphs, independent of inherited button
text alignment. Live/rerun updates no longer short-circuit each other.

Content-chrome windows save their current Qt geometry instead of stale native
resize-cache bounds, preserving the restored size across restarts.

Validation: Release build and 23 focused tests pass (channel import, navigation,
live/unread/mention transitions, close-glyph rendering/activation, settings section
layout/navigation, and chat replies). Native Windows checks cover compact and
maximized navigation, real provider emotes, settings categories, empty search,
reopening and X activation. Authenticated live updates and incoming personal
mentions require a signed-in account and were not exercised with the anonymous
portable test profile.
# User cards and moderation revision

LoginDialog retains its QDialog lifecycle and original account authorization
handlers, but replaces native chrome and visible tab headers with Choppa controls.
The selected login page controls the stack height to avoid a large empty Basic page.
User cards use a compact avatar, borderless painted background, own titlebar,
separate moderation buttons and a stretching recent-message area. Pinned cards
receive ContentChrome through the optional DraggablePopup flags argument.

An unmodified right-click on a UserInfo link now routes to the context menu before
the legacy mention shortcut. The menu keeps mention/profile operations and adds
timeout presets, ban reasons and unban. It resolves the message's actual channel,
validates command targets, excludes self/broadcaster targets, checks rights when
opening and again before executing, and dispatches through the existing command
controller/API handlers. Kick skips sub-minute timeouts. Existing message deletion
and configured command menu items remain reachable.

Validation includes 24 focused tests and anonymous Windows UI checks. No real
moderation requests were sent during validation.
