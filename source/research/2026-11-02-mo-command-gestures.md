# MO command gestures: implementation reference and integration

Question: reproduce fuzzy attack, area target allocation and freehand patrol from
`MagicShiba/MO_ExperienceEnhancements` inside the existing Toolbox Runtime.

Reference revision: `33ed558bd38f36b5012eddcc9c0fb109cc68e38a` (2026-11-02).
Files: `src/order/FuzzyAttack.cpp`, `SmartArea.cpp`, `SmartOrder.cpp`,
`PatrolCircle.cpp`, `OrderUtil.{h,cpp}`, `src/core/EngineAddr.h`, `Hotkey.md`.
The reference tree and README contain no license grant. This implementation
recreates the behaviour using Toolbox's existing native event gateway; it does
not import the reference translation units or its queue writers.

Evidence type: upstream source inspection plus existing Toolbox ABI descriptors.
No new executable/disassembly evidence or real-game multiplayer evidence was
collected in this change. Supported EXE SHA-256 sets remain those recorded by the
baseline's constructor/profile verification; this note adds no new EXE profile.

## New runtime behaviour

- Ctrl+middle: latest 4x4 block, ordinary Attack orders in balanced groups of
  approximately four. Native per-unit target capability is checked at preview
  and immediately before submission; no guessed target/mission setter.
- Shift+middle: scan each sampled stroke block, deduplicate cell-content chains,
  assign garrison slots round-robin, then squads of four and nearest compatible
  remaining targets. Engineer repair/neutral capture and empty vehicle entry
  are limited to one; current occupant/passenger capacity is reserved locally.
- Z+middle: click for a four-cell square, open stroke for a narrow rectangle,
  closed freehand stroke for a native closed planning path. Whole strokes are
  resampled to at most sixteen waypoints, with at most 256 actors and 64 input
  nodes. Invalid map corners reject before automatic-job cancellation.
- Alt and Ctrl+Z do not claim these gestures. Bare middle belongs to the
  separate quick formation integration. Shared `claimsMiddle()` is also used
  by host pointer/hotkey arbitration, preventing a second polled order.
- Inputs are processed only by the existing GameRuntime message and leased
  game-thread entry. Manual revisions, identity checks, match/House changes,
  feature disable and timeout cancel the unsubmitted tail. Sticky/held units
  are excluded; cancellation does not invoke `ForceMission`.
- Preview carries world endpoints plus the current `B0CD48` height projection
  scale. The host projects it through its existing overlay and clipping path;
  it never renders into native game surfaces.

## Native planning ABI and staging

`EventClass` constructors remain `0x4C6860` (MegaMission), `0x4C6780`
(Waypoints/PlanConnect), `0x4C66C0` (NoOptions/PlanCommit). Constructor adapter
accepts the new operational Waypoints case only for event `0x2A`, a positive
Techno actor/tag `0x34`, and an in-range cell destination/tag `0x0B`. Other
Waypoints uses remain diagnostic-only.

1. Each waypoint and actor uses a standard MegaMission with planning byte 1;
   destination and follow both name that waypoint.
2. Observe each live actor's planning token at `+0x514`, node array `token+8`,
   node count `+0x14`, and first node pointer before proceeding.
3. Standard PlanConnect (`0x2A`) connects each actor to the first new cell.
4. Observe positive closed-loop count at `token+0x90`, then standard
   PlanCommit (`0x2B`).

All events pass `nativeMission()` or `submitConstructed()` and the existing
central transport (singleplayer DoList, multiplayer OutList). No token graph,
received-event, House, FrameInfo or CRC memory is rewritten by this feature.
The standard Z key release is temporarily deferred during graph construction
and replayed once complete so native UI release does not commit a half-built
graph. The frame arbiter and global outgoing budget remain authoritative.

Rejected approaches: direct local graph closure (`0x639740`), direct mission
writes, private OutList writer, invoking ClickedMission, extending native
waypoint limits in multiplayer, or assuming virtual capability queries prove
that a queued command executed.

Unresolved until real-game checks: existing closed/native planning graphs may
reject appends according to the original engine semantics; load/scene transitions,
deferred Z release and manual takeover during a large partly built graph need
game validation. The bounded state machine stops rather than guessing if token
materialization or loop closure is not observed within its deadline.
