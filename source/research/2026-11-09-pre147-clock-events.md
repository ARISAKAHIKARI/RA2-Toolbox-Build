# 1.0.47-pre.9: publication-driven observation and idle clocks

Question: 1000 Hz observation repeatedly wakes against a roughly 60 Hz game;
reduce empty work across clocks without tying high-refresh presentation/input
to game logic. This is Host/Runtime infrastructure work, with no new engine
offset, executable ABI, hook or game instruction modification.

## Changes

- Runtime creates an optional auto-reset publication event, named by game PID,
  Host PID and process-connection generation. Only a committed new descriptor
  or active-frame retirement signals it. Re-exporting the identical snapshot
  and refreshing observation metadata do not signal.
- One main observation worker owns the wait. Other consumers continue sharing
  immutable tokens. Events coalesce and the consumer always validates the
  newest sealed publication. Event handles remain alive throughout each wait.
- `Recurring` supports an optional signal source and a semantic configuration
  predicate. Publication wakes promptly; unchanged state only needs a 50 ms
  maintenance deadline. Old/no-event Runtime and unavailable local event wait
  fall back to 250 Hz. Projection, world, reveal and connection changes force
  work; camera-only changes remain the renderer's job. Exception backoff,
  shutdown, invalidation and lease expiry remain independent of notifications.
- `DeadlineWake` can wait for change, publication and timer together. Timer
  creation/arming failure still waits on change/publication with a timeout.
- Detached Assist frame clock parks; existing 250 ms UI discovery remains.
  Attached control remains 16 ms; mouse input and drawing retain their rates.
- Empty input manager changes 250 ms maintenance to 1000 ms, remains periodic
  and restores the active period during attach. Failed cleanup targets remain
  active. No one-shot timer rearm assumptions are introduced.
- Overlay focus/position/expiry maintenance changes 8 ms to 16 ms. Publication
  notifications and precise upload deadlines remain independent; inactive and
  empty overlays still park. No high-refresh or GPU cadence cap is added.

## Evidence and limits

`assist/tests/clock_events_test.py` executes actual production Recurring and
DeadlineWake Win32 branches over controlled event/timer boundaries and real
threads. In normal/timer-create-failed/timer-arm-failed modes, 60 paced new
publications each produced 61 jobs including startup; a 210 ms pause produced
4 maintenance jobs. It covers event coalescing, publication before wait,
in-flight invalidation, source replacement, lease expiry, missing notification
fallback, failed waits and exception storms. No active handles remain.

`world_frame_notifications.cpp` uses actual Client/Server and controlled named
mappings/events: creation failure/retry, absent-capability fallback, 1000
same-frame exports without extra signals, newest-token coalescing, retirement,
reconnection and pinned handles. `overlay_clock_runtime.cpp` executes actual
frameClock in normal/failed-timer modes and checks detached park/stop/resume.
`host_input_test.py` checks immediate attach resume and retained cleanup.

Counts are controlled jobs/notification calls, not measured native wakeups,
CPU utilization, live game latency, GPU FPS or multiplayer validation. Source
capture still uses the existing normal 4 ms game-message trigger and deduplicates
by logic frame; source work, full transport copies and shared-scene contention
can still dominate latency. End-to-end real-game P95/P99 remains unmeasured.

Reference semantics: Microsoft CreateEvent/SetEvent document retained auto-reset
signals with no waiter; ChangeTimerQueueTimer documents the restriction on
rearming an expired one-shot timer. The input manager deliberately stays periodic.
