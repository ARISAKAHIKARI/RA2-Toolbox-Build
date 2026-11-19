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


## 2026-11-19 · Shared-memory topology and actionable sampling logs
The previous estimate `650 reads/sample × 60 samples/sec = 39,000 calls/sec`
is a *hypothetical upper workload*, not a measured call rate and **not
39,000 cross-process Host calls**. The production command worker is in the
injected GameRuntime and reads via
`ReadProcessMemory(GetCurrentProcess(), ...)`. It follows scattered
Techno/Type/House/Cell pointers; a one-call memcpy from one contiguous
"world" virtual address cannot replace those scattered reads without
additional native heap/engine support.

The completed `Snapshot::pages` buffer is already **one source** and is
bulk-published by the pinned publication worker through shared-memory
double-buffer slots (`Server::publish`: one `memcpy` of
`page_count*sizeof(Page)`, currently `sizeof(Page)==5128`).
Host `WorldFrameScope` can pin this immutable sample and each read of
covered bytes becomes CPU-local `Snapshot::read`, without RPM. A
`Page` contains metadata and validity bitsets besides its 4096-byte
payload; `2.8 MB` payload therefore does not imply only `2.8 MB` of
transport bytes.

Some Host tasks intentionally call `ProcessReader::readLive` for current
UI/permission state; visual-only Host `VisualCache` has an independent
off-thread read domain. These are **not necessarily avoidable** without
breaking current-value guards, and aren't represented by a single Host
reader's metrics.

This PR's five-second Host `sampling-health` / `sampling-work` lines
separate:
- `logicFrames`, `publications`, `frameLag`, `maxFrameLag`,
  `failuresDelta` and `starved` (the publication serial counts strict
  Runtime output, not all candidate completions).
- `workerStarted`, `workerPublished`, `workerExact`,
  `workerCandidate`, `workerStale`, `workerWorldChanged`,
  `workerBudget`, `workerRosterFailed`, `workerGraphIncomplete`
  and `workerRejected`: aggregate counters, not an exclusive
  classification of every rejected request.
- `lastWorkerMs`, `maxWorkerMs`, `lastWorkerCalls`,
  `lastWorkerBytes`, `lastWorkerPages` and
  `capturedFromFrame..capturedThroughFrame`.
- `hostReaderRPMCalls`, `hostReaderRPMRequestedBytes`,
  `hostReaderRPMFailures`, `hostReaderSealedHits`:
  *only the main Host `externalReader` ProcessReader session*, not
  independent task-local sessions. Lifetime totals survive `begin()`,
  allowing low-overhead five-second deltas.

The worker only increments thread-local/atomic counters; Runtime transfers
a fixed-size summary at most 4 times/sec; **all disk logging is on Host**.
An append-only IPC v112 tail carries diagnostics; Host and x86 Runtime
must be distributed together and validated against
`ASSIST_SHARED_BYTES` in `verify_windows_outputs.py`.
No feature logic uses diagnostics to authorize native commands.

### Next performance gate
Before changing the capture algorithm, collect 30–60 seconds of
`sampling-work` under a busy scenario. Report
worker calls/graph, worker completed graphs/sec, candidate vs exact,
worker P95/P99 if timestamps are added, Host RPM outside the sealed main
reader, and game-thread `controlPeakUs`. Only then decide whether
type/House/visual domains can be segmented into safe high/low-frequency
read sets, avoiding duplicate game reads while retaining tech-change
invalidation and exact native permissions.
