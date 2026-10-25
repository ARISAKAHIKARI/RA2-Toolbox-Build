# Minimap shroud display policy

Question: the minimap must follow the selected shroud display option; half-transparent main-view shroud means no minimap shroud.

Implementation baseline: `04fd222f0912f0c0c7c55d3f694c6018c6c9986a`, pre.7 sources. This is a production-code/fixture finding, not new reverse engineering of an executable. Existing supported image SHA-256 remains `7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6`; it was not executed for this finding. No new function addresses, ABI or engine offsets are introduced.

## Cause and change

`radar::Sampler::step` accepted the game's display mode `renderReveal` directly. Mode 2 skipped the native exploration test but then halved the color of every unexplored/fogged cell. This made half-transparency in the main viewport also darken the minimap, contrary to the requested behavior. The minimap caller also omitted the native-reveal strategy while the watch caller already included it.

Normalize all enabled preview-reveal modes to one unmasked policy and remove that extra halving. Native mode retains the existing explored/fog visibility rules. Both schematic and real-scene modes consume the same immutable terrain bank. The caller now includes the native-reveal strategy consistently with watch windows.

The immutable bank carries its reveal policy. Switching policy invalidates completed/in-progress rendered backgrounds while retaining decoded tile and overlay art. The worker cannot reuse a completed scene from another policy. Generation tokens continue to reject superseded work. This avoids carrying an exposed background back into native-shroud mode during a multi-pass background refresh.

## Evidence and limits

- `assist/tests/minimap_overlay.cpp`: the new mode-2 assertion failed against the old sampler (SIGABRT); modes 1/2/3 then produce identical unmasked colors and real-scene cell data. Switching back to native restores unseen-cell omission and fog darkening. Existing atomic-bank and geometry regressions remain.
- `assist/tests/radar_scene.cpp`: a 4,097-cell policy change requires two render passes. The first pass must not return a completed background from the previous policy. Decoded source-art load counts remain unchanged.
- `assist/tests/minimap_rate_test.py`: the production worker still preserves its completed frame and warm caches during rate-only updates.

These tests use synthetic memory/art and execute the production sampler, renderer and worker fixture. They do not prove Windows composition, native pixel-exact shroud shapes or multiplayer behavior. No original game art, executable or raw capture is included.
