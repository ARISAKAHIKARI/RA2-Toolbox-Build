# Runtime action / Host display sampling split — 2026-11-23

## Decision

- Every feature that can issue an order or mutate game state is computed and admitted in Runtime on the game thread. This includes Advanced Attack, Magnetron rotation/global pull/drop, Ore Return, Defense, Enter, Aircraft Spread, auto-load/unload/repair and the existing automation paths.
- Runtime builds the same-frame world index and performs final live UID, owner, mission and eligibility checks immediately before native execution. A Host observation or delayed candidate is never command authority.
- The full-world `CommandCaptureWorker` is disabled by default. `RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=1` is retained only as an explicit benchmark/regression opt-in.
- `AsyncPublisher` is not a command-capture worker: it transports already sealed Runtime bytes and performs bounded missing-page probes. Recovered bytes only identify ranges; Runtime re-reads them before any action. Publication uses a separate non-authoritative snapshot buffer, deduplicates unchanged frame/revision publications, and if all publisher slots are busy skips Host publication rather than preventing the Runtime command sample.
- Minimap, secondary camera, overlays and reservation-panel display use Host `VisualCache`. This read-only path may lag, performs world-level coherence checks, and is never used to authorize commands. Nearby same-world display frames may reuse a visual snapshot for up to 32 ms.
- When display is the only demand, Runtime publishes a zero-page frame boundary; Host captures the visual graph. When actions and display are both active but Host has no page requests, Runtime still publishes only a zero-page display boundary and keeps the full action snapshot local. Host publishes a full page copy only when it has explicit non-visual page requests.

## Why

The all-off-thread action path added a round trip between observation, planning and command execution. That latency is unacceptable for timing-sensitive automation such as Magnetron auto-drop and makes ordinary control feel delayed. It also coupled display refresh to the command observation path.

This split keeps action timing and validation in the game process, while moving only low-urgency, read-only display sampling to the Host. It intentionally does not try to eliminate all work from the game thread.

## Validation

- `fixed_compute_settings.py` and `host_frame_demand.cpp`: all action planners are Runtime-owned; Host has no action-planner frame demand.
- `async_command_no_live_roster.py` and `runtime_action_display_split.py`: same-frame Runtime indexing, final live UID checks, opt-in command worker, and Host VisualCache wiring.
- `display_capture_offthread.cpp` / `display_capture_runtime_wiring.py`: display-only boundary and lease behavior.
- `world_frame_async_windows.py`: immutable publisher and bounded recovery worker lifecycle.
- `world_frame_command_worker_windows.py`: synchronous default plus explicit async opt-in on x86/x64.
- Windows x86 Runtime + x64 Host build and in-game regression remain required before release.
