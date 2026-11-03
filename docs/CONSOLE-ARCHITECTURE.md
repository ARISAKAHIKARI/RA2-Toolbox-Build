# Console architecture

The x64 console has one visible WebView2 interface. Its STA thread owns browser
objects and window input; it does not read module HWNDs, write configuration
files, render game scenes, or wait for module commands to finish.

## Responsibilities

| Component | Responsibility |
| --- | --- |
| `suite/frontend` | Declarative settings view, keyed updates, local editing drafts, navigation, search, folds, keyboard gestures, viewport-sized DOM chunks |
| `console_model.h` | Typed, bounded JSON messages, immutable state snapshots, acknowledged actions, thread-safe queues |
| `web_console.cpp` | WebView2 lifecycle and the asynchronous document/native message boundary |
| `modern_console_main.h` | Startup, navigation, module lifecycle, state delivery, persistence status and bounded shutdown |
| `console_adapter.h` | Each module's owning-thread compatibility port, setting values and dispatching existing module commands |
| `async_ini_store.h` | One background INI writer, owned write snapshots, ordering barriers, write-after-read cache and visible failure state |

The original module controls remain hidden compatibility adapters because their
existing configuration and command handlers still use them. They are not
embedded in the browser, captured as screenshots, or used as visible input
surfaces. New setting groups, scrolling and folds belong to the WebView model.
Removing these compatibility controls is a later backend-model migration, not
a requirement for the visible console to remain independent of module work.

GameRuntime and the x86 broker have no WebView2 or console protocol dependency.
Their game command bridge and wire protocols are unchanged by this UI migration.

## Message and state rules

Only typed settings operations cross the UI boundary. Messages cannot select an
arbitrary HWND, call a native window message, read process memory, or invoke a
game function directly. Registered opaque control tokens identify a setting's
lifetime; page/session generations reject an old document's actions.

Actions have monotonic sequence numbers and explicit success/failure replies.
Accepted operations retain FIFO order, including when navigation or a native
dialog follows an edit. Native control access and original command dispatch
always execute on that module's thread. Dynamic list identities are checked
again before applying a selection.

The browser keeps local drafts independently of background refreshes and
updates mounted controls by stable key. A bounded latest-state slot replaces
obsolete presentation snapshots. Slow module work can delay an acknowledgement,
but does not block navigation, text input or layout in the visible console.

The shell receives change notifications instead of polling and repositioning
three settings windows. Its only periodic timer is the short shutdown timer.
Each service samples the selected settings view at a bounded cadence; game
rendering and control scheduling remain in their existing services.

## Persistence and shutdown

Host profile writes copy strings into a bounded queue. Sets for the same key
can be combined; deletion and flush operations are ordering barriers. The
worker uses native per-key INI writes so unknown settings are retained. Pending
or failed values have a read-after-write cache; committed entries return to
native reads so external edits are not permanently masked.

An action acknowledgement means the module accepted the setting, not that a
disk write or a game-side action completed. The console separately displays
configuration saving and failure state. Closing first completes module final
saves, then drains and flushes the writer within the existing shutdown policy.

## Runtime and validation

Build with Zig 0.13.0, Python 3 and the pinned official WebView2 SDK through
`source/build-zig-x64.sh`. Only the redistributable x64 loader and its notices
ship beside the application. The target Windows account needs the Microsoft
Edge WebView2 Evergreen Runtime; startup detects a missing runtime and provides
installation guidance before starting feature services.

Portable tests exercise message validation, FIFO admission, immutable snapshots,
editing state, persistence ordering and failures. Win32 fixtures exercise real
controls, owner threads and Unicode INI files. The browser fixture exercises
the complete view with a controlled backend, including large settings lists.
None of those checks is evidence of live game, multiplayer, or Windows WebView2
visual acceptance.
