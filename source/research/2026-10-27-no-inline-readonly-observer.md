# No-inline: read-only OutList observation prototype

Base: fe5c00bc26ec0a6a02b27a3d4322f123b8b1c859.
Branch: experiment/no-inline-game-patches-20261027. No release/main changes.

## Product changes

The previous native_mega_gateway_bridge.h inline entry is removed in this branch.
The existing NativeMegaGateway setting now enables diagnostic-only OutList sampling
inside the existing leased Assist tick. No new Windows hook or game instruction patch
is installed. The guarded action ABI is retained as an inert function.

A sample contains ring metadata, all event bytes and send timing slots, the local
House/scenario/match identity, frame and toolbox queueSent serial. Metadata/context
are checked again after reading. A later visible suffix is reported only when the
retained prefix matches and no toolbox event was sent between samples. Drains,
changed prefixes, time gaps, local context changes and own traffic are not attributed
to a player. Invalid reads clear the observation baseline.

Only local, ordinary, unexecuted MegaMission candidates are counted. MegaMissionF,
planning appends, foreign House events and other event types remain opaque. No task,
lease, revision, mission, target, queue or game field is changed by this observer.
It writes only its own snapshot history and the existing diagnostic IPC counters.

## Meaning of the UI and legacy IPC fields

The option is labelled 原生命令只读观察（不接管自动任务）.
- nativeMegaGatewayAccepted: visible local ordinary-command candidates; not commands
  proven to be human-generated, accepted remotely or executed.
- nativeMegaGatewaySkipped: uncertain/invalid/context-reset intervals; NOT a number
  of lost or discarded network events.
- status 1: sampling; 0: off/waiting; -3: sample failure; -4: non-MP session.

The status adapter subclasses only the toolbox's own STATIC controls. It does not
subclass game windows or rewrite game code. Seven patch-dependent controls are
disabled and labelled/documented as unavailable. Stored settings are not erased.
Real residual-patch/conflict diagnostics take precedence over the UI annotation.
This does not finish capability gating for every map/technology/aircraft mode.

## Explicit limits

Polling cannot reconstruct an event already consumed between snapshots. Even a
byte-identical pair can hide a complete consume/refill (ABA) cycle. A visible suffix
is diagnostic evidence, not proof of complete event history or human intent.
Therefore this prototype never cancels an automatic task and is NOT an equivalent
replacement for the old successful-append arbitration callback.

WH_CALLWNDPROCRET is not a universal paired post-callback for WH_GETMESSAGE input.
The prototype deliberately does not assume such pairing or change either existing
Windows callback. References:
https://learn.microsoft.com/en-us/windows/win32/winmsg/hooks
https://learn.microsoft.com/en-us/windows/win32/winmsg/getmsgproc

Automatic loading, ore return and repair keep their existing event gateways.
Reliable automatic-task yielding still requires separate evidence and implementation.
No anti-cheat, exception suppression or transport/House bypass is added.

## Validation in this change

Local Linux g++ C++17 with -Wall -Wextra -Werror:
- outlist_observer.cpp: 33,185 assertions, including every physical wrap point,
  full queues, unchanged snapshots, drains/resets, prefix and timestamp changes,
  own traffic, identity/session/frame changes and opaque event rejection.
- outlist_observer_runtime.cpp: 29 integration assertions against the actual new
  bridge header using an in-memory read fixture; game snapshots remain identical.
- no_inline_status_ui.cpp: 30 UI-policy assertions using mocked Win32 controls;
  candidate/uncertain labels and residual/conflict precedence are checked.
- no_inline_observer_contract.py: source restrictions on observer side effects.

The pure observer also passed address/undefined-behaviour sanitizers locally.
These are synthetic tests, NOT live YR, actual GUI screenshots or two-peer gameplay.
Windows CI rebuilds the product and repeats the tests. Its result must be checked
for the final commit; the prior binary must not be called the new observer build.
Binary and diagnostic artifacts are separated; failed jobs do not upload a product
artifact. Concurrent obsolete runs on this branch are cancelled by workflow policy.

## Clean-process requirement and "restore owned patches"

The common manager still permits Restore only for a currently owned, verified patch.
That is cleanup, not permission to install a new patch or overwrite another DLL.
A new runtime does not inherit an old runtime's registry. This prototype installs no
native-Mega patch, so a clean process has none to restore for this component.
Restart the game when changing runtime builds and replace EXE/DLL as a matched pair.
No whole-image byte-identity or multiplayer stability claim is made here.
