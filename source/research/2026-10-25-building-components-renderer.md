# Independent building components in the real-scene renderer

Date: 2026-10-25. Implementation baseline: `v1.0.38-pre.3`, commit
`4485b0a0e3508bba0191049ba6d0dc3df5913792`.

Supported game executable SHA-256:
`7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`.

## Question and evidence boundary

The user reported missing building bibs, factory/naval-yard bases and production
animations after the pre.3 animation repair. The earlier repair collected
`BuildingClass::Anims[21]`, damage-fire slots and global `AnimClass` instances.
It did not collect the independent SHP fields which `Building::Draw_It` draws
without creating an `AnimClass`.

This note records the renderer implementation and public regressions. The
native offsets, ABI, instruction hashes, controlled x86 cases and private
capture inventory are recorded separately in
[native building components](2026-10-25-native-building-components.md). The
capture is read-only evidence; neither its memory nor its game art belongs in
the repository. Fixture pixels test the production decoding/composition code,
not original-game screenshot equality or Windows presentation.

## Confirmed independent paths

`Building::Draw_It` is at `0x43D290`. The direct SHP paths, supplied by
`BuildingTypeClass`, are:

| Component | Type offset | Native selection/frame | Renderer treatment |
| --- | --- | --- | --- |
| Bib | `+0x1518` | Present and `BState +0x534 != 0`; original `GetCurrentFrame` | Ground plane, depth write, bias `+1` |
| Deploying body | `+0x14E4` | Current mission `16`, normal exit | Replaces body; standing/BUILDNGZ, bias `+20` |
| Roof deploying body | `+0x14FC` | Mission `16`, roof exit | Replaces body; standing/BUILDNGZ, bias `+40` |
| Under-door base | `+0x14EC` | Mission `16`, normal exit; healthy `0`, damaged `1` | Ground plane, depth write, bias `0` |
| Under-roof base | `+0x1504` | Mission `16`, roof exit; healthy `0`, damaged `1` | Ground plane, depth write, bias `0` |
| Door | `+0x150C` | Mission `16`; live transition timer and `DoorStages` | Ground plane, no depth write, bias `+5` |

The renderer biases above express the native negative Z adjustments in the
existing `Surface` coordinate system, whose baseline already accounts for the
height projection. They are not new camera angles or XY offsets.

The native call order is body (`0x43D85F`), Bib (`0x43D8E9`), UnderDoor or
UnderRoof (`0x43D9C9` / `0x43DA67`), then Door (`0x43DF3F`, additional drawing
path). Existing image attachments can preserve this order. Introducing a
general "draw every base first" pass would disagree with these call sites.

The roof selector (`0x43D33A` and radio-link getter `0x65AD40`) examines radio
link zero through `building +0xE4/+0xE8`. Its linked techno type has `JumpJet
+0xD94` or `BalloonHover +0xD6A`. The factory's queue item is not the selector.
Missing/invalid radio storage does not select roof art.

Every main building shape is clamped at `min(GetCurrentFrame, shape.frames/2)`
in `0x43D7A7–0x43D7F0`, including the normal body and make art. The endpoint is
`half`, not `half-1`. Bib continues to receive the unclamped original frame.

`SpecialZOverlay +0x1510` and its adjustment `+0x1514` were not added to the
renderer: the native audit found loading/initialization/serialization uses but
no use in this building drawing path.

## Door time and damage

The door reads the building's `TransitionTimer` at `+0x350`, not an animation
slot and not a separate guessed frame counter. `PercentageDone 0x4A52F0` uses:

- `+0x358`: start simulation frame; `-1` means stopped.
- `+0x360`: time left at that start.
- `+0x364`: transition duration.
- `+0x368`: active transition.
- `+0x369`: direction/end-state flag.
- Global `0xA8ED84`: current simulation frame.

An inactive transition or zero duration gives progress `1`. Otherwise remaining
time is the stored time left when stopped, or `max(0, timeLeft - (now-start))`.
Progress is `(duration-remaining)/duration`. The door starts with truncated
`progress * DoorStages (+0xF00)`, complements it when active in the reverse
direction, and uses zero for the inactive closed state. It then clamps to
`[0, DoorStages-1]`. The implementation limits `DoorStages` to `1..4096` so the
optional damaged range stays inside the decoder's `8192`-frame limit.

Damage uses the effective runtime `ConditionYellow` double at
`[0x8871E0]+0x1700`, with existing Rules INI/default fallback if unavailable.
Equality is damaged: `health/strength <= ConditionYellow`. This also corrects
the previous body-frame comparison, which used strict `<`. Door adds a complete
`DoorStages` range only when `DamagedDoor +0x1700` is enabled. The under-door
base uses static damage frame `0` or `1`; it has no independent animation clock.

## Buildup and placement

`Building::GetImage 0x4513D0` selects normal body art only when both
`BState +0x534 != 0` and `ActuallyPlacedOnMap +0x6E4 != 0`. Otherwise it calls
the make-art path (type `+0xE00`, filename `+0xE5C`). `visual::pose`, asset
capture and the per-frame building pose now agree on this condition. The
existing `variant=-2` cache separation prevents a make image from remaining
after placement, or a normal body from replacing the make image too early.

The native capture contains an actual healthy `YAREFN` with `BState=-1` and
`ActuallyPlacedOnMap=0`, so treating only state zero as make art was a concrete
reader gap. This observation does not establish that the object was already
visible. Vanilla Bib visibility remains controlled by BState. Phobos's extra
placement gate is a separate extension and was not silently applied to the
supported executable's behavior.

## Resources, palettes, depth and unavailable parts

`ActorArt` retains six independent `BuildingPart` resources. Each uses its own
live SHP reference filename, disk/MIX fallback and loaded-memory decoder. Bib
can additionally use the type's declared `BibShape` with the current theater
fallback. A missing door filename never falls back to the main building's
`Image` name. Parts share the building palette and the existing scene lighting;
their baked SHP shading is not relit with a voxel directional-light formula.

The byte budget includes all component SHPs. Per-object flags (constructed,
unloading, roof, damaged) and door frame live in `visual::Pose` and both image
cache keys. Source art stays shared by type/owner/variant. Two factories with
the same body frame can therefore render different doors without rereading or
duplicating their art.

Native `DrawObject 0x705E00` draws each building color image and then its shadow,
even when the color does not write Z. Its shadow branches
`0x706469–0x70649E` / `0x70653E–0x706576` recompute a fixed ground plane instead
of inheriting NormalZAdjust or the production body's `+20/+40` bias. All building
body/part shadows therefore use renderer bias `2`, retain the caller's depth
write choice, and draw after their own color. Inheriting roof bias would lift a
roof shadow over nearer foreground pixels. The separate `AnimClass` shadow
path was left alone.

A missing optional SHP must not hide a body which already decoded. Such a
composition is returned successfully with `Image::incomplete=true`, preserving
the body and available parts. If the selected production body is unavailable,
the readable regular body is retained as an incomplete fallback. Both camera
caches display these partial images and retry them after two seconds while
sharing the captured source art. When the engine fills the same SHP reference,
the next retry can complete the image. Completed images keep normal cache
reuse. Pixel-budget accounting subtracts the replaced partial image before
adding its replacement, including attachment pixels.

## Naval-yard scope

The native type inventory shows that `GAYARD`, `NAYARD` and `YAYARD` have no
independent Bib/Deploy/Under/Door pointers. Their bases are the normal body at
type `+0xA4`; their `ProductionAnim` is slot `8` and idle art is slot `18`, with
other active/special slots described in the native note. The existing absolute
coordinate and `Anim.Value + Type.Start` readers remain the applicable path.

A public fixture now passes a naval-yard body and slot-8 production animation
through `captureActor`, `captureAnimation`, `renderActor` and composition. It
checks displayed frames `0`, `5` and `6`, simultaneous base/animation pixels,
and rejection when the parent slot or parent ID changes. It does not fabricate
a new shipyard-specific door or automatically play inactive production slots.

The private stages have no live naval yard. Stage `03-off` contains one
`NAWEAP` in mission `16`, with a linked `HARV`; its independent factory/Bib
resources are unloaded references, and the memory regions were captured
without synchronizing the game. This establishes a captured factory unload
state, not its component pixels or a continuously observed production sequence.
Neither the capture nor these fixtures establish that a live naval-yard
production sequence has been visually accepted in the game.

## Regression results

The following tests passed locally after the implementation:

- `building_components.cpp`: construction and placement gates; current/queued
  unload mission; radio-link roof selector; opening, closing, stopped and
  inactive transitions; damage equality and damaged-door range; bounded input.
- `game_assets_host_test.py`: the production Win32-boundary fixture, real
  resource-discovery and SHP decoder paths; all six unloaded independent SHP
  references; normal and roof production; native main-frame half clamp; Bib's
  original frame; changing door with static body; component depth/write choices;
  shadow-on roof/foreground occlusion and post-color door shadow; partial Bib
  recovery from newly loaded process memory; construction/placement; naval-yard
  body and slot-8 animation; existing infantry, TMP and animation regressions.
- `watch_host.cpp`: shared source bytes with independent factory door and roof
  poses; make/body cache transition; visible partial cache and delayed completion;
  existing 300-type fairness, retained-pose and body/turret cache checks.
- `radar_scene.cpp`: independently cached factory poses; animation frame zero
  and advancement; visible partial cache and delayed completion; existing scene
  publication, native layer/YSort, resource reset and resize checks.
- `git diff --check`: no whitespace errors in the worktree at verification.

The root release workflow owns the complete portable gate, Windows build/UI
checks, final artifact inspection and publication. Those results are recorded
by that workflow rather than inferred from these Linux fixtures.
