# 2026-11-19 · Default-on command-world asynchronous capture candidate

## Runtime behavior
- GameRuntime's production `CommandCaptureWorker::enabled()` is **true by default**, including when `RA2TOOLBOX_ASYNC_COMMAND_CAPTURE` is absent.
- To revert this build to its earlier synchronous command capture behavior, set **`RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=0` before starting the game/Runtime process**. Restart after changing the environment because the mode is cached at first use.
- `RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=1` explicitly enables the worker.
- `WORLD_FRAME_PLATFORM_FIXTURE` remains a deliberately synchronous fixture, so its legacy behavioral tests do not claim to model production default timing.

## Boundaries retained
- Full `CaptureGraph`, page sorting, RPM and native `CommandRoster` hash construction happen on a low-priority worker thread. A busy worker coalesces requests; command callbacks never wait for it to join.
- **GameRuntime alone** adopts a complete, sealed, strictly matching world/scenario/House/session/frame/epoch/demand sample. Invalid, missing, and late samples are *unknown*, not empty inventories or legal commands. Live native order admission, manual-takeover checks and EventClass production stay on the game thread.
- Snapshot handoff requires **exact game simulation frame** and the existing 80 ms acquisition deadline; default-on does **not** mean cross-frame command authorization. A 60Hz game can advance before an overloaded worker completes. Persistent tasks such as queued Ctrl+D are meant to retry, but successful execution is not guaranteed by an offline CI result.
- If real JJ/CnCNet gameplay shows missing commands, use the explicit opt-out to isolate worker timing without weakening permission checks. No automatic synchronous 16K fallback is added to native hooks in this candidate.

## Verification
- Existing portable command policy, Ctrl+D, missing-page, repair, ore, AI and regression tests remain required.
- The real Win32 command worker DLL/host harness now executes **x86 and x64** in separate processes with environment variable absent, `0` and `1`. The enabled cases exercise off-thread RPM, global vehicle roster mode, rejection of expired and incorrect-mode frames, and nonblocking shutdown. The disabled case checks actual Runtime switch policy.
- Paired x64 Host / x86 Runtime compilation is required. The tested source branch and CI artifacts are **not** a released installer. No game FPS/P95/P99 or multiplayer success-rate claim is made.
