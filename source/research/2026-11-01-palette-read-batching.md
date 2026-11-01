# Palette refresh: reduce small remote reads without dropping freshness checks

The existing live minimap records put considerable time in `palette`, but that stage also includes marker/pose bookkeeping. It is not a measurement of the VXL raster kernel alone. In `camera_assets.h`, animation, particle and projectile palette refresh each run a complete object capture twice through `livepalette::refresh`. The context's `Read` forwards every field access to `Reader::readFresh`, so each small field previously caused a separate remote read.

`paletteActorCapture` now batches the first observations of known object/type fields. Attached building animations also batch the known parent header and animation-slot span. `first_field_reads.h` is scoped to one capture: any repeated or overlapping field read returns to the original reader, including the final ID, type, slot and position comparisons. The two palette resolves receive separate batches. Neither the palette payload reads nor their comparison is removed, and nothing is retained across actors or presentations. A failed speculative block falls back to the original individual field reads. VXL debris already uses block capture and is unchanged.

The pre-existing shared SHP material and VPL expansion reuse are preserved. No game writes, hooks, lower update rate, pose quantization or output-resolution reduction are introduced here.

## Production comparison

`assist/tests/palette_read_cost.cpp` calls the actual `Context::refreshActorPalette` with 800 changing actors: 480 animations (240 attached to buildings), 160 particles and 160 projectiles. Synthetic 32-bit addresses map to ASLR-safe pages owned by the fixture, and each source read uses Windows `ReadProcessMemory`. The game frame stays unchanged while palette bytes change between presentations. Five trials each process eight presentations; the table reports the median per presentation.

| Measure | Before | After |
|---|---:|---:|
| Remote-read calls | 55,201 | 25,601 |
| Bytes requested | 1,484,484 | 2,716,484 |
| Palette payload reads | 1,600 | 1,600 |
| Median time | 13.891488 ms | 7.376725 ms |
| Palette result digest | `a1335ec2d5091d43` | `a1335ec2d5091d43` |

The updated fixture passed 9,636 checks, including palette races, object and parent identity changes, failure at required fields, failure in unused padding, post-batch identity changes, repeated fields and overlapping blocks. The old implementation passes the 9,621 applicable comparison checks. Palette results match exactly; fewer calls read larger spans. This isolated synthetic production-path measurement is **not an in-game FPS result** or a claim that the whole renderer is now fast.

Evidence: `diagnostics/palette-read-cost/paired/{result.json,before.log,after.log}` in the audit workspace. The pre-change `camera_assets.h` snapshot is `diagnostics/palette-read-cost/before/camera_assets.h`, SHA-256 `2362f1ef924650dd948a397d278cec9c0ea85548cc94ded3216118708da00d8f`.

From the repository root:

```text
python source/assist/tests/palette_read_cost.py --zig <zig.exe> --baseline-header <pre-change-camera_assets.h> --out <evidence-directory>
```
