# MO native viewport and sidebar migration

**Current strict build: native zoom, native high-resolution camera fix and
native sidebar hiding are not migrated and are unavailable.** The earlier
native prototype described below depended on game-code/vtable patches. Managed
ownership did not satisfy the no-code-write requirement. The strict Runtime
refuses the entire patch operation; viewport/sidebar bridges return before
native layout, surface, buffer, gadget or render-state mutations. Status
`viewzoom::Unmigrated=-8` and sidebar status `-3` report this limitation.
The strict x86 bridge compiled successfully with `CodeWritesAllowed=false`,
Entry ABI 4 and Runtime ABI 5. Its optimized object has no viewport render/
cursor/install/geometry/raster-preparation or sidebar blit/prepare entry
symbols: the strict gates remove those native prototype paths from reachable
code. This is a compile/object inspection, not a real-game execution claim.

## Strict architecture review

The 17 viewport sites include code entries, instruction operands and the
WWMouse GetCoords vtable slot. The 11 sidebar sites include seven code or
operand substitutions and four entry detours. All belong to the rejected
native prototype; none is reclassified as a no-code-write implementation.
The strict cleanup path clears only DLL/IPC state because this binary cannot
acquire those native leases. It does not invoke Set_View_Dimensions or attempt
to restore native patch bytes.

A genuine replacement can reuse the existing external world renderer:

1. Keep native ViewBounds, clippers, WWMouse, DSurface and Z/A state unchanged.
   Keep a host virtual camera and physical output rectangle separately from
   the sampled native camera used to verify a coherent game snapshot.
2. Use `radar::SceneRenderer::render` and `worldscene::Pipeline::draw` with a
   virtual crop. `NativeCanvas::presentRegion`/GPU region preparation and
   `ViewportCanvas` already render arbitrary source extents into physical
   output. `mapbounds::Bounds::clampCamera` already handles an inverted axis
   by pinning its midpoint, and the canvas fills uncovered pixels with its
   background. Thus host output can be stable for a viewport larger than the
   map without patching the native Clamp_Center. This does not fix the hidden
   native underlay or stop its CPU rendering.
3. Enlarge host output over the original sidebar column; radar-only can keep
   a cutout for the native radar/funds/buttons, while bare mode has no cutout.
   Existing menu/focus/fault handling must restore the native picture when the
   host cannot present a coherent scene.
4. Complete interactive ownership before calling this usable zoom/hiding.
   The current compositor is HTTRANSPARENT/WS_EX_TRANSPARENT, so merely
   painting over the sidebar would still click hidden production buttons.
   `host_game_input::pollPointer` also rejects a non-game WindowFromPoint PID.
   A host input region and shared virtual projection must consume the relevant
   gestures, including selection, orders, camera dragging, building placement,
   repair/sell and superweapon targeting. It must use the existing Runtime
   message/intent path rather than creating another injection or event queue.
5. Validate world cells and object identities in Runtime before issuing
   vanilla intents. `ground_geometry.h`, map bounds, terrain/ramp/bridge height,
   world object snapshots and the existing MO/formation event gateways are
   reusable, but a complete virtual-screen picker and native-click replacement
   do not yet exist. Passing virtual pixels to native ProcessClickCoords or
   changing only message lParam is insufficient: native per-frame WWMouse,
   hover, drag selection and negative/out-of-view points keep another mapping.

Ordinary native camera data updates through the existing camera mailbox may
be useful for navigation, but periodic midpoint writes cannot guarantee that
the original clamp will not overwrite them within a render frame. Likewise,
calling Set_View_Dimensions or copying sidebar pixels from message callbacks
does not replace precise render-phase ownership. No verified native data flag
or public Sidebar Activate/Hide API was found that supplies the missing
behavior. IAT/rdata/vtable redirection is not used as an alternative definition
of strict operation.

Read-only review also checked the persistent note
`2026-10-29-native-render-suppression-feasibility`: there is no established
independent data switch that suppresses native world drawing while preserving
its UI. Existing external rendering remains available, but native zoom,
sidebar hiding and native high-resolution clamp repair are explicitly kept
unavailable until their full host input/presentation replacement is complete.

The following sections preserve the prior prototype's evidence and are not a
claim that the strict build exposes these native features.

Question: add map-only zoom, fit-map, three-state sidebar hiding, and stable
rendering when the viewport exceeds the map, without changing simulation rate
or scaling the native sidebar and cursor.

Reference source: `MagicShiba/MO_ExperienceEnhancements` master commit
`33ed558bd38f36b5012eddcc9c0fb109cc68e38a`, retrieved on 2026-11-02.
Relevant implementations: `src/gui/ScaleBar.cpp`, `FullMap.cpp`,
`src/core/MOExperience.cpp`, `src/gui/VecText.cpp`, and their notes.
The repository tree has no LICENSE/COPYING file; this implementation records
the reference and adapts the observed engine interfaces into the existing
Toolbox lease and managed patch policy, without installing MOExperience.

## Engine interfaces and evidence

The supplied repository's existing CameraClamp compatibility probe remains
required: native `0x6D8640` body (excluding the managed five-byte entry) must
have FNV-1a `A41008F5`. No new executable sample was available in this task.
The historical sample SHA256 values are recorded in the existing notebook and
2026-10-23 zoom note; this migration does not extend their executable coverage.

The following are upstream source facts, not a new disassembly observation:

- `0x886FA0` ViewBounds and `0xB0CE28` cached view rectangle describe native
  raster R. `0x886FB0` window rectangle describes the physical client window.
- `0x4A8960` thiscall(rect*) rebuilds clippers, WWMouse viewport and native UI.
  It must receive physical A for sidebar layout, then the native cache is
  synchronized to R through `0x6D5F60` thiscall(rect*). Camera center fields
  Tactical `+D64/+D68/+D74/+D78` are preserved across the physical layout call.
- DSurface ctor `0x4BA5A0`: thiscall(w,h,1,1), object 0x24 bytes, engine new
  `0x7C8E17`; destructor is vtable[0](delete=1). Lock/unlock are +5C/+60,
  bytes-per-pixel +70, pitch +74, width/height +7C/+80.
- Z/ABuffer ctors `0x7BC970`/`0x410CE0` take the rectangle by value; dtors
  `0x7BCAE0`/`0x410E50`, object 0x30 bytes, width/height +28/+2C.
- Render entry `0x4F4480`, cursor suspension `0x4F4492`, map resume
  `0x4F44CB`, chrome suspend/resume `0x4F44FD`/`0x4F4505`, and map completion
  `0x4F451B` bound the three map passes. Exact displaced instructions are
  listed in `native_viewport_policy.h`; adapters replay them with fixed
  continuations and use the common Capture/SEH/FPU boundary.
- WWMouse vtable `0x7F7B2C`, GetCoords slot +34 (`0x7BA360`); the raw physical
  point remains at +1C/+20. Button messages at `0x6930A0` use a separate
  lParam pointer and need the same transform. Edge-scroll dead zones remain
  physical, so `0x692B60` restores the raw point for that native call.

## Resulting implementation

`view_zoom_bridge.h` keeps physical A and logical R distinct. At non-100%
levels it renders into an independent native surface pair at R resolution,
then nearest-neighbor samples that pair into physical A before UI. This avoids
the zoom-in feedback problem of overwriting the incremental source raster with
magnified output. It does not change screen resolution or invoke DDEnableZoom.
Native Z/A pointers and the exact original `0x887314` pointer are saved per
render and restored for chrome and after map completion. Only owned buffers
are freed; native buffer destruction stays with Set_View_Dimensions.
Recursive map Render calls return immediately while the outer pass owns its
surface lease. Cleanup requested during a map pass is deferred to completion;
the stop path must wait for `nativeViewRetireReady()` before disconnecting IPC.

Map-size clamps reuse the existing midpoint behavior independently per axis.
After native map drawing, pixels outside the map render rectangle are cleared,
not extended from a previous screen edge. The render rectangle uses the
engine's -5/+135 vertical padding, rather than treating the movement boundary
as the drawing boundary.

`native_sidebar_bridge.h` supplies full/radar-only/bare modes and immediately
leases a full-window native surface pair when injection occurs mid-match.
Column geometry, radar copying, input gadgets and native click routing share
the same physical anchor. Original gadget Disabled values are preserved.
Restoration checks surface ownership; an intervening native rebuild causes the
old lease to be forgotten rather than restoring or freeing uncertain pointers.
Two optional native hover-only hooks lack verified original bytes and are not
installed; full-hidden native cursor appearance needs real-game inspection.

The shared viewport arrays publish A/R under a sequence guard. Host rendering,
native queries and external sampling use the common projection helper. Raster
R may exceed physical screen dimensions. Sidebar counters remain in physical
coordinates, and tactical culling uses R while snapshot screen dimensions remain
physical.

## Validation and unresolved scope

The modified x86 bridge and x64 tactical host compiled with Zig 0.13.0.
This validates C++/x86 ABI generation and integration, not actual native
DirectDraw allocation, loading, visual output, click hit testing or multiplayer
synchronization. No Windows game process was run and no real-game pass is
claimed. Full packaging/build validation is recorded by the parent task.

Remaining real-game checks: all nine zoom selections; camera edges and small
maps at 2560x1440/4K; drag scrolling; left/right sidebar families; radar clicks;
native mouse shapes in bare mode; save/load and physical mode replacement;
coexistence with installed CnCNet/Ares/Phobos sites. Unknown or foreign managed
sites are rejected rather than overwritten.
