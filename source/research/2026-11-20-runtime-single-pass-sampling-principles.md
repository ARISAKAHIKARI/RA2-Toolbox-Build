# 2026-11-20 · Runtime single-pass sampling architecture and cutover

## Architecture decision

The target is one Runtime-owned acquisition pipeline that produces immutable, complete world snapshots. The project will not preserve old acquisition paths merely to keep every feature working during migration. A feature that cannot consume the new snapshot is temporarily unavailable; it is not allowed to trigger its own sampler or force a synchronous fallback.

The pipeline is:

1. The game callback samples only the small world/session identity needed to describe the request, admits bounded page-demand metadata, and submits/coalesces a request without waiting.
2. One Runtime-owned worker performs the complete CaptureGraph, page coalescing/reads, graph traversal, UID roster/index construction, and snapshot sealing.
3. A sealed snapshot is published through the existing shared-memory transport. Host consumers import and reuse that immutable token; they do not independently walk the game object graph.
4. Read-only candidate snapshots may help discovery/forecasting within explicit age/world bounds. They never authorize a native command.
5. Immediately before a native order, the game thread validates the current object's UID/address identity, owner/House, life/mission/manual state, target identity, and command admission. This is execution safety, not a second sampling architecture.

A full graph is captured once per accepted world sample, not once per feature. The sampling cadence is driven by the simulation frame and active demand; it is not an unconditional full scan at 60 Hz while no consumer needs it.

## Non-negotiable rules

1. **One production acquisition owner.** Runtime's command worker is the only production reader that constructs the game-world graph. No feature-specific sampler.
2. **Complete graph, one sample.** Coalesce adjacent pages and reuse each captured page within the sample. Do not distribute a single world graph over unrelated per-feature polling loops.
3. **Immutable handoff.** Convert native structures into a compact sealed snapshot and UID index. Consumers do not retain native pointers as long-lived identities.
4. **No game-thread waiting.** No blocking sampler mutex, worker join, full-graph fallback, or second full scan on the game callback. If the worker is busy/unavailable, defer or fail closed.
5. **No silent partial success.** Incomplete pages, roster holes, world changes, stale tickets, and address reuse remain explicit unknown/failure states. Never reinterpret unknown as absent/dead/completed.
6. **Commands require live proof.** A prior/candidate snapshot is discovery data only. Native command submission revalidates current UID, owner, life, mission, manual version, target and destination.
7. **No code-patching sampler hooks.** Do not require inline/detour hooks, game-code modification, or create/destroy/property hooks for sampling. This is important for JJ/CnCNet/platform compatibility.
8. **Transport is not sampling.** Shared-memory double-buffer publication is a copy/ownership boundary. It must not trigger another independent game-memory walk.
9. **Suspend, do not emulate.** If an old feature depends on a conflicting acquisition route, suspend that feature until it is migrated. Do not add compatibility branches to the new core.
10. **Measure the entire path.** Record worker P50/P95/P99, pages/calls/bytes, failed/incomplete samples, frame lag, demand-list cost, publication cost, and game-thread callback P50/P95/P99.

## Current production cutover in this branch

- CommandCaptureWorker is unconditionally enabled in production. The old RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=0 escape hatch is removed. The synchronous CaptureGraph implementation is compiled only into explicit test fixtures (WORLD_FRAME_TEST_SYNC_CAPTURE / WORLD_FRAME_PLATFORM_FIXTURE).
- The production worker continues to use ReadProcessMemory(GetCurrentProcess()) and the existing page cache/coalescing. Direct memcpy and VirtualQuery + memcpy remain benchmark-only; neither is promoted without a fault boundary and lifetime analysis.
- The Host VisualCache and its independent full CaptureGraph are disabled in product builds. WORLD_FRAME_TEST_VISUAL_CACHE exists only for regression fixtures. Host display scopes that require this second graph fail closed until their fields are included in the Runtime snapshot.
- The zero-page HostDisplayOnly boundary route is suspended in product builds. A boundary-only marker is not a substitute for a complete snapshot.
- RecoveryMailbox targeted side-channel reads are compiled only into a test fixture (WORLD_FRAME_TEST_RECOVERY). Production missing-page retries flow back into the next complete Runtime graph request; a second background sampler does not probe individual fields.
- ShadowGraphWorker is compiled only into a test fixture (WORLD_FRAME_TEST_SHADOW). It is not a second production graph reader.
- Existing SampleReadScope consumers continue to read the strict sealed Runtime snapshot. Observation-only candidates remain separate and cannot issue EventClass/native commands.
- The former bridge.cpp indexWorld full-roster fallback is removed from production. Command consumers bind only the worker-built strict/candidate UID index; if neither is admissible, the affected task stays pending. Missile hold cleanup uses the previously owned object pointer plus a fresh live UID/House/life check rather than rebuilding the entire roster.
- Command-maintenance managers now require a strict same-frame worker roster. Candidate-only indexes remain usable for discovery paths that perform explicit live proof, but they cannot sweep leases, retire dodge/unload state, prune manual revisions, or admit native planner orders. When the strict roster is missing, those jobs pause instead of interpreting an unavailable UID as a destroyed object.
- The game callback still performs small world/session identity checks and builds a page-demand list. This is not yet the desired final cost: explicit request caps, overflow accounting, reusable ownership-swapped buffers, and worker prestart remain the next P0 engineering tasks.
- Final live identity/permission/mission checks and native EventClass/engine calls remain on their required game thread. They are not a compatibility sampler and must not be moved to the worker.

## Temporarily suspended feature domains

| Domain | Current decision | Re-enable condition |
| --- | --- | --- |
| Host overlays depending on VisualCache (unit/passenger/combat/resource visual graph) | Suspended; no Host-side game graph reads | Runtime snapshot contains required display fields and Host consumes the same immutable token |
| Host-only zero-page display boundary | Suspended | Display demand is represented as a normal Runtime snapshot demand |
| Targeted RecoveryMailbox page probes | Suspended in production | Recovery is integrated into the same complete-sample scheduler and cannot publish a second source |
| Shadow full-graph diagnostic reader | Test-only | Only if a future diagnostic build is isolated from normal production and cannot compete with the canonical worker |
| Any legacy feature that performs its own full roster/world enumeration outside the sealed sample | Suspend that feature at its admission boundary | Collector migrated to snapshot; unknown/incomplete behavior and live command proof covered by tests |

This is an intentional temporary loss of affected read-only displays or features. It is preferable to silently running multiple incompatible sampling systems. Do not re-enable them by restoring the old path.

## Remaining implementation stages

### P0 — Finish the canonical worker path

- Prestart the command worker from a safe Runtime initialization point, outside DllMain and before gameplay callbacks. The first request must not create a thread/event while holding the request gate.
- Explicitly cap runtimeWorldDemands before merge. Deduplicate and validate the union of Host/runtime/lease pages before expensive work. Report overflow as a first-class diagnostic; never build an unbounded list only to reject it at the worker.
- Replace queued_.pages.assign(...) with reusable request-buffer ownership transfer. The game callback should submit a bounded descriptor/lease, not copy a second large vector.
- Remove remaining production code for synchronous CaptureGraph, HostDisplayOnly, and legacy recovery. Test-only fixtures must be clearly compile-time isolated.
- Audit every feature entry: identify any live read used for discovery/planning and either migrate it to the canonical snapshot or suspend the feature. Keep only small world identity reads and final command validation on the game thread.

### P1 — Decide the production read backend

The benchmark is test-only and compares full CaptureGraph + CommandRoster using:
- direct in-process memcpy (lower bound);
- VirtualQuery preflight + memcpy (not race-safe by itself);
- current ReadProcessMemory(GetCurrentProcess()).

It runs identical synthetic contiguous and scattered layouts, verifies byte-identical sealed pages/UID indexes, and reports P50/P95/P99. Hosted CI timing is diagnostic, not a real-game FPS claim.

Keep RPM as production default until a guarded direct reader:
- contains access faults without modifying game code;
- passes x86 Runtime / x64 Host ABI and native worker tests;
- handles unreadable pages, concurrent decommit/protection changes, shutdown/restart and address reuse;
- materially improves full-graph P95/P99 on realistic page layouts;
- does not increase incomplete samples, command failures, game-thread stalls or JJ/CnCNet disconnects.

VirtualQuery followed by memcpy has a check/use race. SEH can contain a fault but does not prove object lifetime or multi-field atomicity. If direct reads do not materially outperform RPM, retain RPM.

### P2 — Migrate suspended features

- Add display-only fields/demands to the canonical Runtime graph and transport; do not reintroduce Host VisualCache.
- Fold missing-page retry into the same graph scheduler; do not reintroduce a separate field-probe reader.
- Migrate feature collectors one by one. Re-enable only after incomplete/unknown semantics, object identity, stale-frame behavior, and command admission have regression coverage.
- Preserve existing user-facing options where possible, but a temporarily suspended option must report unavailable rather than pretending to work.

### P3 — Real-game acceptance

Collect at least 30–60 seconds in a busy single-player scenario, then repeat on JJ and CnCNet:
- worker P50/P95/P99, pages/calls/bytes, exact/candidate ratio, frame lag, failures/incomplete;
- game-thread callback P50/P95/P99, request assembly, publication and first-start cost;
- Ctrl+D, auto-load, auto-repair, ore return, tactical collectors and manual order success/latency;
- disconnects, stale object/address reuse, world/session changes, pause/resume and shutdown.

CI success is not gameplay acceptance. This branch remains a draft; no merge to main or release is implied.
