# Observation-window lifetime and reader capability audit

The left observation window could lose a manually pinned object for two independent reasons. This change corrects both and preserves existing explicit-close, confirmed death/succession, process exit and world-retirement behavior. It does not install a game hook or write to the game.

Supported-layout reference: YR 1.001 `gamemd-spawn.exe`, SHA-256 `268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`. This audit uses the documented layout and production reader fixtures; it does not execute the private game binary. Reused world globals are Scenario `0xA8B230`, session `0xA8B238`, map array/count `0x87F924/0x87F928`, frame `0xA8ED84` and running flag `0xA8E9A0`. No native function ABI or code patch is added.

## Confirmed lifetime defects

`external_reader/source/watch_focus.h::life` had `ObjectClass +0x74` and `+0x90` reversed. The native fields are `IsOnMap` at `+0x74` and `IsAlive` at `+0x90`. An alive, temporarily off-map object with neither Limbo, transport nor cloak set could therefore become `Life::Dead`; two such agreeing samples caused `watch::follow` to erase the pin. The fixed field order is tested with asymmetric byte values, rather than an ordinary object whose two flags are both one. The native layout is documented by [fixed YRpp ObjectClass](https://github.com/CnCNet/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/ObjectClass.h).

`watch_host.h` also treated every change of `displayMatch()` as destruction of all tracked identities. An unavailable external sample makes that helper fall back to the command bridge's independent epoch. Furthermore, `TacticalSampler::sample` clears its cached House after a failed layout/House-list read, so recovery can advance its display serial within the same Scenario. Neither condition proves the original objects were destroyed.

The watch Host now uses the existing checked, read-only `worldlife::capture` and a separate watch lifetime. PID replacement, confirmed Scenario/map-array/session replacement, frame rewind or an entirely empty world clears pins. Unknown reads preserve pins and suspend presentation. A recovered display epoch in the same verified world invalidates old worker results and rebinds their generation while retaining the pinned identities. This gate samples the world once per new external observation, rather than adding fresh remote calls to every paint. It never substitutes the command epoch for unavailable display identity. Focus and pause are not retirement criteria.

## Confirmed avoidable work

The current watch renderer wrapped its `ReadSession` in `std::function<bool(uint32_t,void*,size_t)>`. That erased the reader's `prefetch` member. The common terrain Sampler selected its no-op batching fallback, despite the real reader supporting adjacent-page batching. `watch_read_view.h` retains the callable read behavior and forwards only that existing bounded batch operation. A reader without batching still uses the former scalar path. Fresh identity reads and double-sample checks are unchanged.

The main-view observation/terrain readers and minimap already use `SharedReadView` and retain `prefetch`; this is not a claim that all three previously missed batching.

Repeated watch presentations also fed the same immutable SceneBank publication into `Timeline::observe`, recopying its full actor inventory. `watch_observation.h` skips only that duplicate input copy when the shared publication object is identical. A new publication with an unchanged engine frame still updates membership and poses. Motion sampling, camera construction, native palette/light verification and world preparation continue at the requested presentation cadence.

## Isolated validation

`tools/test_watch_pipeline.py --zig <zig.exe> --build-dir <directory>` compiles the actual production headers and extracts the current Host `synchronizeWorld` function for its entry test. It does not open or run RA2.

* `watch_lifecycle.cpp`: 20 checks covering alive/off-map, unknown reads, recovery, actual death, original-group succession, pointer reuse, display-epoch changes, frame rewind, world replacement and PID retirement.
* `watch_reader_batch.cpp`: actual `Sampler` and `ProcessReader`, using the fixture process's own private memory. A 128×128 map contains 32,640 physical/material Cells. Eight complete fresh sweeps compare every color, mask and CellArt field. Old `std::function` input performs 22,728 remote-read API calls; the typed adapter performs 2,592, down 88.6%. Measured sweep time is 165.443 ms versus 142.475 ms, down 13.9% on this run. A `PAGE_NOACCESS` page causes the candidate to fail and retains the last complete bank; restored access recovers normally.
* `watch_observation.cpp`: 120 motion/facing outputs equal the original Timeline output; same-frame publication replacement and reset remain correct. Refeeding the same 4,095-actor publication 800 times costs 132.743 ms in the old input-copy stage and 0.014600 ms in the owner-identity guard. This is an input-copy microbenchmark, not an end-to-end frame-rate claim.
* Existing `watch_renderer_idle_windows.cpp`: the real renderer still performs zero game reads for disabled/suppressed empty cameras and takes the validated sampling path for pinned/automatic demand.

Logs and production SHA-256s are in the build evidence directory `diagnostics/watch-lifecycle-performance`. No RA2OL/live-game FPS or synchronization result is asserted.

## Remaining preparation cost

Watch still prepares a complete retained world, once per update shared across its three camera windows. Skipping that preparation merely because the SceneBank pointer or game frame is unchanged would miss independent native light/converter/palette changes. This patch does not use such a shortcut, reduce refresh frequency, crop world content or lower rendering quality. Further preparation reuse needs a complete dynamic-input contract and separate profiling.
