# Native building component, production and buildup selection

## Question and supported evidence

Investigate missing building Bib graphics, war-factory exit/roof graphics and
door motion, naval-yard base/production animation, and construction-yard make
animation. Establish which parts are ordinary `AnimClass` slots and which are
independently drawn SHPs. The investigation does not change game simulation.

Supported executable SHA-256:
`7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6`.
The private capture declares image base `0x400000`, image size `7942144`, PE
timestamp `1004491854`, machine `332`. Original instruction bytes come from the
mapped executable, with captured validity masks checked before hashing.

Evidence:

- `assist/tests/native_bib_capture.py`: validity-checked private-capture loader,
  original x86 execution and sanitized configuration inventory.
- `verification/native-bib-capture.json`: three stages, **414 native cases per
  stage, 1,242 total**, all assertions passed; SHA-256 of each executed code
  range. The test accepts an extracted private session directory or its ZIP.
- The prior baseline is `v1.0.38-pre.3`, commit
  `4485b0a0e3508bba0191049ba6d0dc3df5913792`; prior related evidence is
  `2026-10-25-renderfix3-async-native-depth.md` and the persistent reverse-
  engineering notebook entries searched before this investigation.
- Pinned primary-source field-name cross-checks:
  [BuildingTypeClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingTypeClass.h),
  [BuildingClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/BuildingClass.h),
  [TechnoClass.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/TechnoClass.h),
  [TransitionTimer.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/TransitionTimer.h), and
  [GeneralDefinitions.h](https://github.com/Phobos-developers/YRpp/blob/d9795546950fcc75a94325ffc83b44d16ec34ce2/GeneralDefinitions.h).
  Critical offsets and branches below were also checked in the supported
  executable; the headers are not the sole evidence for them.

No executable, raw memory, extracted game image or private asset is included in
the repository. The emulator runs selection/frame instruction slices and native
scalar helpers. Map lookup, height-query and `TechnoClass::DrawObject` are
intercepted at their boundaries in the component probes; the draw ABI is
recorded. The actual rasterizer, disk loader, allocation, animation constructor,
simulation updates, Win32 presentation and multiplayer are not executed.

## Independently drawn SHPs

`BuildingClass::Draw_It` is `0x43D290` (building vtable `+0x114`). The following
fields belong to `BuildingTypeClass`, not `BuildingClass::Anims[21]`:

| Component | Type offset | Native condition / displayed frame | Caller Z adjustment | Gradient / write Z |
| --- | --- | --- | --- | --- |
| BibShape | `+0x1518` | Non-null and building `BState +0x534 != 0`; `GetCurrentFrame()` | `-1 - HeightProjection` | Ground / yes |
| DeployingAnim | `+0x14E4` | Mission 16, normal exit, non-null: replaces body | `-20 - HeightProjection` | Deg90 / yes |
| RoofDeployingAnim | `+0x14FC` | Mission 16, roof exit, non-null: replaces body | `-40 - HeightProjection` | Deg90 / yes |
| UnderDoorAnim | `+0x14EC` | Mission 16, normal exit; healthy frame 0 / damaged frame 1 | `0 - HeightProjection` | Ground / yes |
| UnderRoofDoorAnim | `+0x1504` | Mission 16, roof exit; healthy frame 0 / damaged frame 1 | `0 - HeightProjection` | Ground / yes |
| DoorAnim | `+0x150C` | Mission 16, non-null; independent transition timer below | `-5 - HeightProjection` | Ground / no |

The body/replacement displayed frame uses `GetCurrentFrame` (`0x43EF90`), with
an upper clamp at `selectedShape.FrameCount / 2` in `0x43D7A7..0x43D7F0`.
This clamp is **half**, not half minus one. The Bib call uses the native current
frame directly, without that body clamp. Underlays use damage state and do not
have an independent animation clock.

Main draw order is body (`0x43D85F`), Bib (`0x43D8E9`), underlay
(`0x43D9C9` or `0x43DA67`). The door is a later independent draw
(`0x43DF3F`). The name "UnderDoor" does not imply moving its call before the
body. The original Bib branch at `0x43D874` checks BState only; an additional
ActuallyPlacedOnMap gate in an extension hook must not be mistaken for a
condition present in this original instruction slice.

The roof selector (`0x43D33A..0x43D386`) calls `0x65AD40(this, 0)` to obtain
`RadioLinks[0]` from building `+0xE4`, then checks the linked object's
`GetTechnoType` (`vtable +0x84`) flags `JumpJet +0xD94` or
`BalloonHover +0xD6A`. This differs from the separate clipped drawing method
at `0x43D030`, which consults the factory object. The main draw must not infer
roof choice from factory identity or a type-wide cached pose.

`0x705E00` is the thiscall `DrawObject` helper, returning with `ret 0x40`.
Its 16 arguments are SHP, frame, point, bounds, two existing scalar draw
parameters, Z adjustment, gradient, write-Z flag, brightness, tint, Z shape,
Z frame, Z offset X, Z offset Y, and flags to remove. At
`0x706129..0x70613E`, a gradient other than -1 adds `0x2000`; write-Z adds
`0x4000`. Body/replacement use BUILDNGZ; Bib, underlay and door pass a null
Z shape. The passed screen point has no additional component-specific X/Y
offset.

`SpecialZOverlay +0x1510` has initialization and resource-loader references;
`SpecialZOverlayZAdjust +0x1514` also has INI/serialization references. A full
text-section operand scan and the building drawing paths found no native draw
read of these fields. This is not evidence to add a new visible overlay layer.

### Shared helper shadows

The shared helper selects shadow frame `shape.FrameCount / 2 + displayFrame`
(`0x7064BB..0x7064CB`, also `0x70658D..0x70659D`), subject to the global shadow
switch and the techno type's NoShadow flag. It draws the visible shape before
its shadow. Shadow flags retain the caller's write-Z bit: the shadow branch
clears only translucency bits with `and edi, 0xfffffff9`, then adds `0x601`.

Shadow Z is recomputed independently in `0x706469..0x70649E` and
`0x70653E..0x706576`: `-2 - HeightProjection`, followed by the shared `-2`
blitter adjustment. It does not inherit NormalZAdjust, -20/-40 for the exit
body, or the Bib/underlay/door adjustment. Giving a roof replacement shadow
the roof's +40 scene-depth bias would incorrectly raise the ground shadow.
In the reader's existing flat plane with lift 2, this shared helper corresponds
to fixed shadow `depthBias=2`. This finding is specific to the techno helper;
AnimClass has a separate drawing path.

## Door timer and damage threshold

The object at building `+0x350` is `TransitionTimer` / `UnloadTimer`.
It is not an `AnimClass` and does not appear in the 21 building animation slots.

| Field | Building offset |
| --- | --- |
| ActionTimer.StartTime | `+0x358` |
| ActionTimer.TimeLeft | `+0x360` |
| Total duration / Rate2 | `+0x364` |
| State1, active | `+0x368` |
| State2, direction | `+0x369` |
| Current game frame | global `0xA8ED84` |
| DoorStages | type `+0xF00` |
| DamagedDoor | type `+0x1700` |

`PercentageDone` (`0x4A52F0`, thiscall) returns 1 when State1 is zero or total
duration is zero. Otherwise:

```text
remaining = StartTime == -1 ? TimeLeft
          : max(0, TimeLeft - (CurrentFrame - StartTime))
progress = (TotalDuration - remaining) / TotalDuration
frame = truncate(progress * DoorStages)
if State1 && !State2: frame = DoorStages - frame
if !State1 && !State2: frame = 0
frame = clamp(frame, 0, DoorStages - 1)
if damaged && DamagedDoor: frame += DoorStages
```

Native predicate helpers are `0x4A5110` (11), `0x4A5130` (10),
`0x4A51B0` (01), and `0x4A51D0` (00); the drawing frame logic is
`0x43DE19..0x43DED9`. The original `_ftol` at `0x7C5F00` uses the executable's
control word at `0x822D80` (captured `0x0E7F`), giving truncation here. The
native code has no 4096-stage validation; the reader's 4096 safety bound is
consistent with its supported 8192-frame SHP limit including damaged frames.

Damage means `health / strength <= ConditionYellow`, using native health
ratio `0x5F5C60` and the RulesClass pointer at `[0x8871E0]`, double at
`RulesClass +0x1700`. Equality is damaged. The ordinary building idle/Bib
frame selection at `0x43F0F6..0x43F117` also uses equality; a reader test using
strict `<` selects the wrong frame at exactly half health under default rules.

## Naval-yard and construction-yard paths

All three captured naval-yard type configurations have null pointers for the
six independent fields above. Their base is normal type `+0xA4`, with
reference names `GGYARD.SHP`, `NGYARD.SHP`, `YGYARD.SHP`; their make resources
are type `+0xE00`. This rejects treating the whole naval-yard base as a missing
Bib or door resource.

| Type | Native configured animation slots |
| --- | --- |
| GAYARD | 5 ActiveThree `_C`; 8 Production `_D`; 10–12 Special `_S1/_S2/_S3`; 18 Idle `_A` |
| NAYARD | 4 ActiveTwo `_B`; 5 ActiveThree `_C`; 8 Production `_D`; 10–12 Special `_S1/_S2/_S3`; 18 Idle `_A` |
| YAYARD | 5 ActiveThree `_C`; 8 Production `_D`; 10–12 Special `_S/_S2/_S3`; 18 Idle `_A` |
| GACNST | 3 Active `_A`; 8 Production `_B` |
| NACNST / YACNST | 3 Active `_A`; 8 Production `_B`; 18 Idle `_C` |

The BuildingType animation configuration starts at `+0xF4C`, stride `0x44`;
runtime `AnimClass*` slots start at building `+0x55C`, stride 4. Slot role
numbers are part of the evidence, so production slot 8 must not be confused
with active slot 3 or the independent door timer.

Native `PlayAnim` slices `0x4518E3..0x451943` and
`0x45197B..0x451A36` confirm all 27 configured slots across these six types,
both normal and damaged variants: absolute position from building
`GetRenderCoords` (`X-128, Y-128, Z`) plus the configured offset; `OwnerObject
+0xCC` remains zero; palette flag `Anim+0x118` becomes one; `Anim+0x100` and
`+0x104` receive configured Z adjustment and Y sort; the animation is stored
in the requested building slot. These slices stop around allocation and
construction. They do not synthesize or advance a game production event.

For every such normal/damaged animation type, original frame instructions
`0x42303F..0x423061` use `Anim.Value +0xAC + AnimType.Start +0x2B4`.
Examples: GAYARD_DD start 41, NAYARD_DD start 41, YAYARD_DD start 17;
shipyard special variants also have nonzero starts. Values 0 and 1 both pass,
including the first animation frame. Native `GetYSort` and `GetLayer` also
pass with these copied settings. The existing generic building-slot reader
must handle them; no separate naval animation clock is justified by the data.

`Building::GetImage` (`0x4513D0`) selects the normal body only when
`BState +0x534 != 0` **and** `ActuallyPlacedOnMap +0x6E4 != 0`.
Otherwise it tail-calls type virtual `+0xC0` (`LoadBuildup`, `0x465960`).
Only checking BState misses the early placement interval. Captured stage
02-on has 51 healthy building objects with ActuallyPlacedOnMap true and two
with it false: YAREFN (BState -1, mission -1) and NALASR (BState 0, mission
18). These object observations are not a claim that both were visible.

Controlled make/sell cases for all six yard types execute `0x43EF90`: mission
18 uses the native Stage value; mission 19 reverses the configured make frame
range. Production animation and construction/selling frames remain distinct
state paths.

## Coverage, constraints and open observation

Per-stage controlled cases:

| Case family | Count |
| --- | ---: |
| Bib state/resource/health | 12 |
| Body resource/mission/roof selection | 16 |
| Radio-link JumpJet/BalloonHover selector | 16 |
| Body half-frame clamp | 10 |
| Underlay damage frame and draw ABI | 8 |
| Underlay resource/mission gate | 16 |
| Door active/direction/time/damage frame | 56 |
| Door paused/zero-duration/future-start/health boundary | 32 |
| Door resource/mission gate | 8 |
| Shared shadow frame arithmetic | 6 |
| Six yard types: BState/placement body selection | 36 |
| Six yard types: make/sell frame | 36 |
| Configured slot coordinate/binding/depth/layer variants | 54 |
| Configured animation Value+Start | 108 |
| **Total** | **414** |

The stages contain 48/53/57 healthy building objects and 17/20/23 types.
Non-null Bib references occur for 3/3/5 live types; exit/roof references occur
for 1/1/2. All these non-null independent references have a zero decoded-SHP
pointer; the capture alone has no pixel payload for them. No live type has a
DoorAnim in this capture. Disk/MIX fallback and retry are required for a
reader to draw these resources when their in-memory decoded image is absent.

Stage 03-off contains a NAWEAP at mission 16 linked to a HARV (neither
JumpJet nor BalloonHover), while 01-off/02-on have no mission-16 buildings.
The capture writes regions sequentially: the module's current frame is 4313
while that object's transition start is 4334, so the capture must not be
treated as a synchronized animation movie. Timer clamping is tested under
this kind of future-start boundary.

No live naval-yard object or active naval-yard production was captured. The
type/animation tables and controlled native cases establish the exact paths,
but cannot establish a live Windows naval-yard pixel match or prove that all
reported missing visuals are fixed. A production pipeline regression can
verify base-plus-slot composition with public fixture art; actual game visual
acceptance requires an observable naval-yard/production sequence. The shader
and renderer findings here do not adjust or determine a camera angle.
