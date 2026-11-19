# 2026-11-19 · Continuous off-thread full-world candidates / pre.5 failure

## Real-game evidence from pre.5
Attached Runtime.log captured on 2026-11-19: within ~76 seconds
`sampleFailures` rose from 4228 to 9167 (~65/s), while the native hook
continued firing, `runtimeError=0` and command status remained 1.
Complete captures were typically 671–683 pages, ~2.75–2.80 MB, ~644–655
ReadProcessMemory operations. The latest successfully published `sampleFrame`
frequently lagged `controlFrame` by tens or hundreds of logic frames.
Ctrl+D reached Runtime but repeatedly produced `result=-7`.
This means the earlier worker admission design starved the native feature
dispatcher, not that the input hook itself stopped working.

## Correct separation of concerns
- **Full background capture**: on every new logic frame, offer the latest
  work request to a low-priority worker; at most one scan is active. Busy work
  is coalesced, no queue grows, and outdated intermediate frames are dropped.
  There is no full Techno roster scan/UID hash construction on the game thread.
  Target is every frame **when scanning fits the budget**; 60 completed full
  graphs/s is not guaranteed when a ~2.8 MB scan exceeds 16.7 ms.
- **Latest complete candidate**: a worker may seal a complete base world graph
  even if the game advances while RPM is running. Keep this cross-frame graph
  for at most 500 ms, only when world, House, match, session, generation and
  connection match. Use its index for **candidate discovery**, not authority.
- **Native command authority**: strict exact-frame, age and demand-revision
  admission is unchanged for snapshots. Every candidate-based ID or pointer
  lookup is rechecked against the live UID; native mission submission still
  performs current House, manual-takeover and mission eligibility verification.
  A stale candidate can NEVER directly authorize an EventClass.
- **Feature scheduler**: it must not stop all active jobs simply because a
  full async graph was not finished in precisely the current 16 ms frame.
  The bounded candidate index lets active automatic functionality continue;
  truly unavailable/expired indexes remain unknown and retry.
- **No high-cost game-thread fallback**: identical requests and running
  full-world scans are coalesced without rebuilding the dependency list.
  New Host demand revisions do not cancel a scan that can still serve as an
  observation; a following scan picks up the updated request. No sync 16K UID
  hash or full graph is added to the normal dispatcher.

## Evidence and remaining gates
Native x86/x64 worker harness explicitly advances the simulated game frame
while the worker is reading and ensures old-frame graph is rejected by strict
`take()` but accepted by observation-only `takeObservation()`. Portable
policy tests verify age/world separation and live UID rejection when the
candidate points to a replaced actor.

Not yet proven: real-game 60Hz completed-snapshot rate, in-game command
success/FPS/P95/P99, or multiplayer JJ/CnCNet determinism. Keep this on a
Draft PR until paired x86 Runtime/x64 Host compilation and native worker
lifecycle tests pass. Do not publish a new package from an unverified SHA.

Emergency workaround for released pre.5: launch the process with
`RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=0` to restore the older synchronous
command-graph path, or revert to pre.4. This is temporary, **not** the target
off-thread architecture.
