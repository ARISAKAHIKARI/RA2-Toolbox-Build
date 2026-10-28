#!/usr/bin/env python3
"""Portable AB experiment regression, without copyrighted game assets."""
import importlib.util
import json
import struct
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/shp_palette_ab.py"
spec = importlib.util.spec_from_file_location("shp_palette_ab", TOOL)
ab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ab)

shp, pal = ab.demo_files()
shape = ab.indexed_frame(shp, 0)
assert shape["width"] == 64 and shape["height"] == 48
assert shape["frame"] == 0 and shape["total_frames"] == 1
assert len(shape["indices"]) == 64 * 48
assert ab.indexed_frame(shp, 0)["indices"] == shape["indices"]
assert ab.decode_pal(pal, False)[1] == 0xFFFCa000
assert ab.decode_pal(pal, True)[1] == 0xFFFFA204
assert ab.decode_pal(pal, True)[0] == 0
assert ab.decode_pal(pal, False)[0] == 0
try:
    ab.decode_pal(pal[:-1], True)
    assert False, "truncated palette accepted"
except ValueError:
    pass
try:
    ab.indexed_frame(shp, 1)
    assert False, "invalid frame accepted"
except ValueError:
    pass

# RLE row semantics, including an actual transparent run.
compressed = bytearray(8 + 24 + 2 + 5)
struct.pack_into("<4H", compressed, 0, 0, 5, 1, 1)
struct.pack_into("<hhHHI", compressed, 8, 0, 0, 5, 1, 2)
struct.pack_into("<I", compressed, 28, 32)
# uint16(length)=7, color 1, 0 skip 2, color 2
compressed.extend(b"")  # explicit reserved row already allocated above
compressed[32:] = bytes([7, 0, 1, 0, 2, 2, 0])
assert ab.indexed_frame(compressed, 0)["indices"] == bytes([1, 0, 0, 2, 0])

# Branch's transparent-neighbor rule remains strict.
k, white = 0xFF000000, 0xFFFFFFFF
assert ab._block(k, 0, 0, white, white, True) == (k,) * 4
assert ab._block(0, white, white, k, k, True) == (0,) * 4
pixels = [white, k, k, white, k, k, white, white, white]
nearest = ab.upscale2x(pixels, 3, 3, False)
edge = ab.upscale2x(pixels, 3, 3, True)
assert nearest[3 * 6 + 2] == k
assert edge[3 * 6 + 2] == 0xFF808080
assert edge[2 * 6 + 2] == k
assert nearest != edge

with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp)
    report = ab.compare(shp, pal, 0, out, "dummy.shp", "dummy.pal", demo=True)
    assert report["synthetic_demo"] is True
    assert report["same_frame_indices_for_dac_rgb"] is True
    assert report["same_rgb888_pixels_for_nearest_edge"] is True
    assert report["sha256_indexed_frame"]
    assert report["color_stats"]["changed_pixels"] > 0
    assert report["color_stats"]["max_absolute_channel_delta"] <= 3
    assert report["scale_stats"]["different_2x_pixels"] > 0
    assert report["frame"] == 0
    assert report["two_x_output"] == [128, 96]
    assert len(report["image_order"]) == 7

    # All 7 PNGs must contain a valid IHDR, IDAT, IEND with valid CRC and
    # decompress to exactly the expected filter-0 RGBA scanline extent.
    for name in report["image_order"]:
        png = (out / name).read_bytes()
        assert png.startswith(b"\x89PNG\r\n\x1a\n")
        pos = 8
        got = {}
        while pos < len(png):
            count = struct.unpack_from(">I", png, pos)[0]
            marker = png[pos + 4:pos + 8]
            payload = png[pos + 8:pos + 8 + count]
            checksum = struct.unpack_from(">I", png, pos + 8 + count)[0]
            assert zlib.crc32(marker + payload) & 0xFFFFFFFF == checksum
            got.setdefault(marker, []).append(payload)
            pos += 12 + count
        assert pos == len(png) and b"IEND" in got
        w, h, depth, color, comp, filt, interlace = struct.unpack(">2I5B", got[b"IHDR"][0])
        assert (depth, color, comp, filt, interlace) == (8, 6, 0, 0, 0)
        raw = zlib.decompress(b"".join(got[b"IDAT"]))
        assert len(raw) == h * (1 + 4 * w)
        assert all(raw[y * (1 + 4 * w)] == 0 for y in range(h))

    recorded = json.loads((out / "AB_manifest.json").read_text(encoding="utf-8"))
    assert recorded == report
    assert (out / "06_palette_AB_same_frame.png").stat().st_size > 100
    assert (out / "07_upscale_AB_same_pixels.png").stat().st_size > 100

print("PASS: 7 real RGBA PNG exports; one immutable SHP frame, native RLE crop, DAC/RGB mapping, identical-pixel 2x A/B, manifest and error guards")
