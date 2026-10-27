# No-inline experiment: bounded observation work

Date: 2026-10-27
Branch: `experiment/no-inline-game-patches-20261027`
Starting commit: `0de6a0e19cef5dfaea7192e4c1c071b9de99be69`

## Scope

This increment changes independent observation work, not event construction, game code-patch policy, network ownership, or simulation mission setters. It does not claim that the entire Runtime tick has a fixed time limit. The formal release branch is unchanged.

## Repair confirmation batches

`repairdelivery::Pending::observe` now resumes from a cursor. A default call inspects at most 256 table slots and invokes at most 32 current-world observation callbacks. Full and sparse tables are both bounded; a zero budget does nothing. The return value reports scanned, sampled and retired entries for tests and future diagnostics.

Unvisited entries remain pending. A delayed sample is never a timeout, proof of execution, or permission to send another Repair toggle. The existing bridge callback still validates object ID, pointer, type and owner against the current world. Invalid identities, repairing state and healthy state remain the existing terminal observations. No prior-tick result is cached. Clearing an empty table is local bookkeeping, not additional game reads. Existing command freshness, final ownership checks and event budgets are unchanged.

This can delay confirmation versus a full scan. It does not establish perfect observation of short-lived state transitions or solve manual cancellation arbitration.

## OutList diagnostics

A small identity/frame check precedes the bulk queue reads. Within a stable observation session, at most one bulk attempt is admitted for the same context and simulation frame. Failed bulk attempts also consume that opportunity; repeated callbacks in the same frame do not repeatedly read the event array. Off/on, context changes and frame rollback can establish a new baseline.

Each admitted queue snapshot is still read as a whole, with header and context rechecks. It is NOT assembled across multiple frames. The final checks now also revalidate the House ID and HouseIndex, rather than relying only on the House pointer. Deferral keeps counters and status unchanged and does not fabricate a snapshot.

This is diagnostic-only and can miss transient events. It does not cancel tasks, identify human commands reliably, modify queue contents, or replace the old successful-append hook. Frame-based throttling can reduce diagnostic coverage; this is an explicit tradeoff for bounded repeated sampling.

## Correct feature classification

The UI adapter distinguishes:

- Shroud/Fog drawing and direct-map patches: unavailable under the no-code-patch policy.
- Native Reveal: still changes exploration data; not silently disabled by that policy and not claimed multiplayer-safe.
- Technology enhancement: changes type data, not game instructions. Its active/restoration/error status must remain visible.
- Compatibility aircraft waypoints: native-event mode, not automatically equivalent to the patch-backed native mode. Still requires gameplay verification.
- OutList observer: only candidate telemetry, not automatic-task takeover.

Saved options, hotkeys, control layout and shared-memory protocol are retained. The status adapter does not install game hooks or alter any game state.

## Validation

Local C++17 GCC tests passed with `-O2 -Wall -Wextra -Werror` for:

- Existing repair-delivery invariants.
- New work budgets: full/sparse/colliding tables, zero/large budgets, round-robin fairness, identity replacement, deterministic randomized model comparison and no timeout retries.
- Mock Runtime observer: repeated same-frame callbacks, bulk-read failures, context/header changes and byte-identical game snapshots.
- UI policy: patch/data/event distinctions, stale-world reveal status and preserved conflict diagnostics.

The Windows workflow also runs the existing registry and OutList pure-policy tests before building the full EXE/DLL. CI logs establish Windows results; this note does not substitute for the run result. No live-game or two-computer multiplayer test has been performed for this increment.
