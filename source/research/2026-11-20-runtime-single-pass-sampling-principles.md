# 2026-11-20 · Runtime single-pass sampling principles

## Decision

The sampling base is a single Runtime-owned acquisition pipeline. Prefer one complete graph capture per accepted sample over slicing the native object graph into per-frame batches. Do not add object-create/destroy/property-change hooks as a requirement: some multiplayer platforms modify or monitor game code and can disconnect when code sections are patched.

The first optimization step is measurement, not switching the production reader blindly. A synthetic full CaptureGraph benchmark now compares:
- direct in-process memcpy (best-case lower bound);
- VirtualQuery preflight plus memcpy (still not safe against a concurrent free);
- current ReadProcessMemory(GetCurrentProcess()).

It runs the same production CaptureGraph, page cache, warm-page coalescing and CommandRoster build against identical contiguous and scattered layouts. It checks byte-identical sealed snapshots and UID indexes. Timings are diagnostic only; hosted CI timing is not a pass/fail threshold and is not a real-game FPS result.

## Current implementation found on the PR #65 head

- On the current PR #65 head, CommandCaptureWorker is enabled by default and runs the full CaptureGraph and roster build on its own low-priority pinned worker. It does not walk the complete Techno roster on the game callback in the default configuration.
- Important current exception: explicitly setting RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=0 restores the legacy synchronous CaptureGraph path on the game callback. That opt-out is outside the target architecture and must be removed or restricted to a deliberate diagnostic build before claiming an unconditional no-main-thread-full-capture guarantee.
- The worker's current live reader uses ReadProcessMemory(GetCurrentProcess()). The Snapshot caches each page; repeated field reads hit the local page image rather than issuing another OS read.
- Adjacent unknown pages are coalesced by Snapshot::prefetchPages (up to 32 pages for actor warmup, 16 by default for mandatory/unknown requests). Non-adjacent pages remain separate; this is a memory-layout constraint, not a reason to invent per-frame object polling.
- The 2026-11-19 real-game log summarized in 2026-11-19-continuous-offthread-candidates.md reported roughly 671–683 pages, 2.75–2.80 MB and 644–655 RPM operations for a completed graph. The estimate 650 × 60 = 39,000 is a hypothetical upper workload, not a measured call rate.
- The worker publishes one sealed graph through the existing shared-memory double-buffer. The producer bulk-copies the page array into the inactive slot; the Host imports a new publication into its local immutable snapshot pool. Consumers reuse that token. This is not a per-field cross-process RPM loop.
- PR #65 also publishes complete observation-only candidates from the worker so Host display/reservation consumers are not starved by strict exact-frame command admission. Observation-only data never becomes native command authority.
- The game callback still assembles the page-demand list. This is not a full graph traversal, but it is not yet proven bounded by MaxPages: runtimeDemandBits covers 8192 × 64 = 524,288 page addresses, while the worker accepts at most 16,384 raw page entries. The merge of Host requests, runtimeWorldDemands and leases occurs before that worker-side size rejection; duplicates can also make the raw count exceed the unique count. This is a concrete game-thread audit item, not something to hand-wave as a 64 KiB copy.
- Worker admission and take/defer paths use try_to_lock on the game callback, so they do not wait for the worker mutex. However, request() still copies the assembled vector into queued_.pages (up to 64 KiB when accepted) and may allocate; the next stage should bound the producer list explicitly and use a reusable ownership-swapped request-buffer pair to remove the second copy.
- The current branch defaults to asynchronous capture. Explicit RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=0 still selects the legacy synchronous CaptureGraph path. The eventual production design should remove that synchronous escape hatch; if the worker is unavailable or behind, the callback must defer/retain a bounded-age observation or fail closed, never silently fall back to full synchronous capture.
- RA2TOOLBOX_SHADOW_GRAPH=1 enables a separate diagnostic background graph reader. It must remain off during normal performance measurements so it cannot double the sampling load.

## Non-negotiable rules

1. Runtime is the only owner of game-memory acquisition. No feature gets its own independent native-memory sampler.
2. Prefer a complete sample in one worker job; this means one complete graph per accepted sample, not an unconditional 60 Hz full scan. Coalesce adjacent pages; read each required page/range once per sample; do not turn this into many independent per-feature reads.
3. Convert native data into a compact immutable snapshot. Features consume the snapshot/local cache, not raw game pointers.
4. Keep graph traversal, page sorting, roster hashing, snapshot packing and publication off the game main thread.
5. The game main thread never waits for the sampler, takes a blocking sampler mutex, or performs a full-graph fallback. If the worker is busy, coalesce/drop intermediate work and keep the last valid observation only within its explicit age/world bounds. The current branch defaults to async; the explicit async=0 synchronous escape hatch remains a known gap.
6. Never retain an unprotected native pointer as a long-lived object handle. Treat address as a locator, not identity; pair it with the native UID/generation where available.
7. A candidate or previous-frame snapshot is for observation/discovery only. Before any command is submitted, revalidate current object identity, owner/House, life state and mission eligibility on the native command path.
8. Handle destruction, reuse, partial reads and world changes fail-closed. SEH/access checks can contain an access fault but do not prove an object was alive or make a multi-field read atomic.
9. Do not add inline/detour hooks or modify game code sections for sampling. No create/destroy/property hook is required for correctness.
10. Shared memory is a transport boundary, not a synchronization excuse: use explicit buffer ownership, sequence/version checks and immutable Host tokens. Consumers may skip stale publications; the producer must not wait for a slow consumer.
11. If a complete capture cannot fit the worker budget, drop/coalesce that capture or publish no new complete sample. Do not move the work back onto the game thread or silently publish a partial sample as complete.
12. Optimize based on measured P50/P95/P99 worker duration, page count, calls, bytes, page hits, failed reads, frame lag and game-thread callback cost.

## Reader selection gate

The benchmark is deliberately test-only. Direct memcpy is not yet a production option: a background thread can race object destruction, page decommit/protection changes and address reuse. VirtualQuery followed by memcpy has a check/use race. A production direct reader would need an isolated SEH fault boundary and a carefully defined policy for failed reads; even then it would not establish object lifetime or snapshot atomicity.

Keep the current RPM reader as the production default until all of the following are true:
- the guarded direct reader is implemented without modifying game code;
- x86 Runtime and x64 Host ABI/build checks pass;
- native x86/x64 worker tests cover unreadable pages, concurrent unmap/protection changes, shutdown/restart and stale UID/address reuse;
- the direct path is compared against RPM on the actual graph shape, not just one large contiguous memcpy;
- real-game logs show a material reduction in worker P95/P99 without increasing failed/incomplete samples;
- single-player and JJ/CnCNet multiplayer tests show no disconnects, command regressions or main-thread stalls.

If the safe direct reader does not materially outperform the current batched RPM path, retain RPM. Do not trade stability for a theoretical memcpy win.

## Staged implementation

- Stage 1 (this branch): document the architecture contract and add a full-graph reader A/B benchmark to the existing x86/x64 native worker CI.
- Stage 2: use the results to decide whether a guarded direct-read backend is worth prototyping. Keep it opt-in and keep RPM fallback/default.
- Stage 3 (priority): explicitly cap runtimeWorldDemands before merge, surface overflow instead of silently dropping or repeatedly rejecting, deduplicate/limit before expensive game-thread work, and hand request-vector ownership to the worker through reusable buffers. Remove the explicit synchronous full-graph escape hatch after command-freshness and failure behavior are proven. Measure the full game-thread callback P50/P95/P99.
- Stage 4: collect 30–60 seconds of real-game sampling logs in a busy scenario; inspect worker P50/P95/P99, exact/candidate ratio, page/call counts, frame lag, incomplete/faults and main-thread P95/P99.
- Stage 5: only after correctness and platform testing, promote a proven backend. No release is implied by this research branch.
