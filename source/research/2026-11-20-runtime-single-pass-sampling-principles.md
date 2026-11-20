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
- The Host VisualCache and its independent full CaptureGraph are disabled in product builds. WORLD_FRAME_TEST_VISUAL_CACHE exists only for regression fixtures. Host display and resource scopes subscribe to the same Runtime snapshot as other consumers. Scene renderers require a publication whose visual demand was captured under stable map geometry, and read the exact token that produced their scene.
- The zero-page HostDisplayOnly boundary route is suspended in product builds. A boundary-only marker is not a substitute for a complete snapshot.
- RecoveryMailbox targeted side-channel reads are compiled only into a test fixture (WORLD_FRAME_TEST_RECOVERY). Production missing-page retries flow back into the next complete Runtime graph request; a second background sampler does not probe individual fields.
- The former AsyncPublisher/PublicationOnly worker is compiled only into the isolated synchronous regression DLL. Product Runtime has one real worker: CommandCaptureWorker; the production WorldFramePublication seam is a zero-cost no-op, not a dormant fallback thread.
- ShadowGraphWorker is compiled only into a test fixture (WORLD_FRAME_TEST_SHADOW). It is not a second production graph reader.
- Existing SampleReadScope consumers continue to read the strict sealed Runtime snapshot. Observation-only candidates remain separate and cannot issue EventClass/native commands.
- The former bridge.cpp indexWorld full-roster fallback is removed from production. Command consumers bind only the worker-built strict/candidate UID index; if neither is admissible, the affected task stays pending. Missile hold cleanup uses the previously owned object pointer plus a fresh live UID/House/life check rather than rebuilding the entire roster.
- Command-maintenance managers now require a strict same-frame worker roster. Candidate-only indexes remain usable for discovery paths that perform explicit live proof, but they cannot sweep leases, retire dodge/unload state, prune manual revisions, or admit native planner orders. When the strict roster is missing, those jobs pause instead of interpreting an unavailable UID as a destroyed object.
- The game callback now builds a deduplicated page-address union capped at MaxPages (16,384), fails closed on invalid/oversized requests, and transfers the vector by ownership swap. The worker returns its previous Work buffer through the queue slot; Runtime reclaims it before assembling the next request, avoiding repeated vector growth after warm-up. Remaining P0 cost: first worker startup still creates its event/thread from request() on the gameplay callback, and the bounded union/bitset work still needs real callback timing.
- The async path does not start the command worker unless the Host heartbeat/subscription is live. Retained leases survive disconnect for later retry but do not start an idle worker.
- Full Techno-list reads in selection filters, idle-unit selection, type selection, autoload capacity accounting, burst discovery, advanced-range discovery, cooperative entry capacity and Ctrl+D ejection watching now consume the sealed Runtime roster/sample. The selected-unit overlay also reads its selection vector from the sealed sample; global overlay projections remain suspended.
- Final live identity/permission/mission checks and native EventClass/engine calls remain on their required game thread. They are not a compatibility sampler and must not be moved to the worker.

## Temporarily suspended feature domains

| Domain | Current decision | Re-enable condition |
| --- | --- | --- |
| Host overlays previously depending on VisualCache | Main render, minimap, watch cameras, and reservation forecasts use Runtime publications. Read-only consumers retry missing pages through the same worker | Validate field coverage, recovery and capture duration for each overlay in real games |
| Global Iron/Psychic/AllTransports/advanced-order label projection | Suspended; selected-unit projection remains, sourced from the sealed Runtime sample | Worker publishes a dedicated display projection; Host/game thread does not rescan the full Techno list |
| Variant-key type selection | Temporarily returns pending/unsupported (-12); ordinary type selection uses the sealed worker roster | Variant weapon keys are projected by the Worker and live CanSelect proof remains on the game thread |
| Host-only zero-page display boundary | Suspended | Display demand is represented as a normal Runtime snapshot demand |
| Targeted RecoveryMailbox page probes | Suspended in production | Recovery is integrated into the same complete-sample scheduler and cannot publish a second source |
| Shadow full-graph diagnostic reader | Test-only | Only if a future diagnostic build is isolated from normal production and cannot compete with the canonical worker |
| Any legacy feature that performs its own full roster/world enumeration outside the sealed sample | Migrate to SampleReadScope/worker roster or suspend at its admission boundary | Collector migrated to snapshot; unknown/incomplete behavior and live command proof covered by tests |

This is an intentional temporary loss of affected read-only displays or features. It is preferable to silently running multiple incompatible sampling systems. Do not re-enable them by restoring the old path.

## Remaining implementation stages

The [PR #71 feature audit](2026-11-20-pr71-feature-sampling-audit.md) records production entry points, live discovery and display exceptions, and work that still runs on the gameplay callback while reading sealed bytes.

### P0 — Finish the canonical worker path

- Prestart the command worker from a safe Runtime initialization point, outside DllMain and before gameplay callbacks. The first request must not create a thread/event while holding the request gate.
- [x] Cap runtimeWorldDemands and the Host/runtime/lease union at MaxPages. Deduplicate and validate before submission; overflow fails closed and never publishes a truncated graph. Overflow currently increments worldSampleFailures; a dedicated diagnostic can be added without changing the shared IPC ABI.
- [x] Replace queued_.pages.assign(...) with vector ownership swaps. The worker keeps the in-flight Work buffer across jobs and returns the previous allocation to the producer queue; Runtime reclaims it before the next request is assembled.
- [x] Remove the old AsyncPublisher worker from product builds. It remains only in the isolated synchronous regression DLL; production has one canonical command-capture worker.
- [x] Remove the production synchronous CaptureGraph fallback, HostDisplayOnly boundary capture, and RecoveryMailbox side channel. Their regression fixtures are compile-time isolated.
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

- [x] Route display and resource demand through the canonical Runtime graph. Tag scene publications only when visual coverage was requested and map geometry stayed stable. Include native sidebar fields with reservation resource demand.
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
