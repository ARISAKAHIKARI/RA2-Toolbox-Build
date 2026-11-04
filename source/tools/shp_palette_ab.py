#!/usr/bin/env python3
"""Offline RA2 indexed SHP A/B: ONE source frame, ONE PAL, no game process access.

Examples:
  py source/tools/shp_palette_ab.py --demo --out ab-demo
  py source/tools/shp_palette_ab.py --shp path/to/unit.shp --pal path/to/unit.pal --frame 4 --out ab-unit

Outputs exact-pixel DAC vs RGB888 and, from the SAME RGB888 pixels, 2x
nearest-neighbor vs the branch's conservative edge-guided half-blend.
This is an asset/frame comparison, NOT a claim of live-game screenshot identity.
"""
import argparse
import hashlib
import json
import math
import struct
import zlib
from pathlib import Path

MAX_SOURCE_PIXELS = 1024 * 1024
MAX_FILE_BYTES = 8 * 1024 * 1024
RGB_DISTANCE_LIMIT = 3 * 24 * 24


def _load(path):
    path = Path(path)
    if not path.is_file() or path.stat().st_size > MAX_FILE_BYTES:
        raise ValueError("missing or oversized local asset: " + str(path))
    return path.read_bytes()


def indexed_frame(data, frame):
    """Decode TS/YR SHP frame including FA2-compatible length-prefixed rows.

    The cropped image keeps its exact frame offsets; transparent index 0 is
    not treated as a visible black pixel. The global canvas is NOT a crop box.
    """
    if len(data) < 32:
        raise ValueError("SHP header too short")
    tag, canvas_w, canvas_h, frames = struct.unpack_from("<4H", data)
    if tag != 0 or not (1 <= canvas_w <= 2048 and 1 <= canvas_h <= 2048):
        raise ValueError("not a supported file-backed TS/YR SHP")
    if not (1 <= frames <= 8192) or len(data) < 8 + 24 * frames:
        raise ValueError("bad SHP frame table")
    if not (0 <= frame < frames):
        raise ValueError("frame index out of range")
    at = 8 + 24 * frame
    x, y, w, h = struct.unpack_from("<hhHH", data, at)
    flags = struct.unpack_from("<I", data, at + 8)[0]
    offset = struct.unpack_from("<I", data, at + 20)[0]
    if w > 2048 or h > 2048 or abs(x) > 4096 or abs(y) > 4096:
        raise ValueError("frame bounds exceed supported SHP limits")
    width, height = max(1, w), max(1, h)
    if width * height > MAX_SOURCE_PIXELS:
        raise ValueError("AB source frame exceeds 1 million pixels")
    indices = bytearray(width * height)
    if offset and w and h:
        if offset < 8 + 24 * frames or offset >= len(data):
            raise ValueError("SHP pixel offset invalid")
        pos = offset
        compressed = bool(flags & 2)
        for row in range(h):
            if compressed:
                if pos + 2 > len(data):
                    raise ValueError("SHP compressed row header truncated")
                length = struct.unpack_from("<H", data, pos)[0]
                if length < 2 or length > 8192 or pos + length > len(data):
                    raise ValueError("SHP compressed row length invalid")
                run = data[pos:pos + length]
                pos += length
                i = 2
            else:
                if pos + w > len(data):
                    raise ValueError("SHP raw row truncated")
                run = data[pos:pos + w]
                pos += w
                i = 0
            col = 0
            while i < len(run):
                color = run[i]
                i += 1
                if compressed and color == 0:
                    if i == len(run):
                        raise ValueError("SHP RLE missing skip length")
                    col += min(run[i], max(0, w - col))
                    i += 1
                    continue
                if col >= w:
                    raise ValueError("SHP row contains excess opaque literals")
                if color:
                    indices[row * width + col] = color
                col += 1
    return {
        "width": width, "height": height, "canvas_width": canvas_w,
        "canvas_height": canvas_h, "crop_x": x, "crop_y": y,
        "frame": frame, "total_frames": frames, "indices": bytes(indices),
    }


def decode_pal(raw, full_rgb):
    if len(raw) != 768 or max(raw) > 63:
        raise ValueError("PAL requires 768 bytes with every RGB component <=63")
    result = [0] * 256
    for i in range(1, 256):
        r, g, b = raw[i * 3:i * 3 + 3]
        if full_rgb:
            r, g, b = ((v * 255 + 31) // 63 for v in (r, g, b))
        else:
            r, g, b = (r * 4, g * 4, b * 4)
        result[i] = 0xFF000000 | r << 16 | g << 8 | b
    return result


def apply_palette(indices, palette):
    return [palette[i] for i in indices]


def _rgba(pixel):
    return ((pixel >> 16) & 255, (pixel >> 8) & 255,
            pixel & 255, (pixel >> 24) & 255)


def _chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))


def encode_png(pixels, width, height):
    """Portable RGBA PNG, filter 0. No Pillow or third-party DLL required."""
    if not (1 <= width <= 8192 and 1 <= height <= 8192):
        raise ValueError("invalid PNG dimensions")
    if width * height > 16 * 1024 * 1024 or len(pixels) != width * height:
        raise ValueError("invalid PNG image size")
    data = bytearray()
    for y in range(height):
        data.append(0)
        for p in pixels[y * width:(y + 1) * width]:
            data.extend(_rgba(p))
    ihdr = struct.pack(">2I5B", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr)
            + _chunk(b"IDAT", zlib.compress(data, 5))
            + _chunk(b"IEND", b""))


def _similar(a, b):
    if a >> 24 != b >> 24:
        return False
    if not (a >> 24):
        return True
    ar, ag, ab, _ = _rgba(a)
    br, bg, bb, _ = _rgba(b)
    return ((ar - br) ** 2 + 2 * (ag - bg) ** 2
            + (ab - bb) ** 2 <= RGB_DISTANCE_LIMIT)


def _blend(a, b):
    if (a >> 24) != 255 or (b >> 24) != 255:
        return a
    r0, g0, b0, _ = _rgba(a)
    r1, g1, b1, _ = _rgba(b)
    return (0xFF000000 | ((r0 + r1 + 1) // 2) << 16
            | ((g0 + g1 + 1) // 2) << 8 | ((b0 + b1 + 1) // 2))


def _block(c, u, l, r, d, edge):
    if not edge or not (c >> 24) or _similar(u, d) or _similar(l, r):
        return (c, c, c, c)
    def pick(a, b):
        return _blend(c, a) if _similar(a, b) else c
    return pick(u, l), pick(u, r), pick(d, l), pick(d, r)


def upscale2x(pixels, width, height, edge):
    if pixels is None or len(pixels) != width * height or width * height > MAX_SOURCE_PIXELS:
        raise ValueError("invalid source image")
    dest_width = width * 2
    result = [0] * (dest_width * height * 2)
    def get(x, y):
        return pixels[y * width + x] if 0 <= x < width and 0 <= y < height else 0
    for y in range(height):
        for x in range(width):
            c = pixels[y * width + x]
            if not c:
                continue
            a, b, cc, d = _block(c, get(x, y - 1), get(x - 1, y),
                                  get(x + 1, y), get(x, y + 1), edge)
            at = (y * 2) * dest_width + x * 2
            result[at] = a
            result[at + 1] = b
            result[at + dest_width] = cc
            result[at + dest_width + 1] = d
    return result


def _sheet(left, right, width, height, gap=12):
    """Same-size A/B tiles. Source coordinates are identical across the gap."""
    if len(left) != len(right) or len(left) != width * height:
        raise ValueError("AB sheet must use aligned, identical extents")
    result_w = 2 * width + gap
    result = [0xFF22252B] * (result_w * height)
    for y in range(height):
        at = y * result_w
        for x in range(width):
            # Checkerboard is display-only. Raw PNG outputs retain alpha.
            checker = 0xFF333842 if ((x // 12 + y // 12) & 1) else 0xFF4A505A
            a, b = left[y * width + x], right[y * width + x]
            result[at + x] = a if (a >> 24) else checker
            result[at + width + gap + x] = b if (b >> 24) else checker
    return result, result_w


def _palette_stats(before, after):
    changed = 0
    sum_abs = 0
    max_abs = 0
    opaque = 0
    for a, b in zip(before, after):
        if not (a >> 24):
            continue
        opaque += 1
        diffs = [abs(x - y) for x, y in zip(_rgba(a)[:3], _rgba(b)[:3])]
        if any(diffs):
            changed += 1
        sum_abs += sum(diffs)
        max_abs = max(max_abs, *diffs)
    return {
        "opaque_pixels": opaque, "changed_pixels": changed,
        "changed_pct": round(100 * changed / opaque, 4) if opaque else 0,
        "mean_absolute_channel_delta": round(sum_abs / (3 * opaque), 6) if opaque else 0,
        "max_absolute_channel_delta": max_abs,
        "note": "No ground truth: differences measure color mappings, not fidelity.",
    }


def _difference(before, after):
    out = []
    for a, b in zip(before, after):
        if not (a >> 24):
            out.append(0)
            continue
        diff = [min(255, abs(x - y) * 64) for x, y in
                zip(_rgba(a)[:3], _rgba(b)[:3])]
        out.append(0xFF000000 | diff[0] << 16 | diff[1] << 8 | diff[2])
    return out


def compare(source, palette_bytes, frame, destination, source_name="unit.shp",
            palette_name="unit.pal", demo=False):
    shape = indexed_frame(source, frame)  # decoded ONCE, immutable across A/B
    indices = shape["indices"]
    w, h = shape["width"], shape["height"]
    dac = apply_palette(indices, decode_pal(palette_bytes, False))
    rgb = apply_palette(indices, decode_pal(palette_bytes, True))
    nearest = upscale2x(rgb, w, h, False)
    edge = upscale2x(rgb, w, h, True)
    # Display matched DAC and RGB at the SAME 2x nearest-neighbor magnification.
    dac_big = upscale2x(dac, w, h, False)
    diff = _difference(dac, rgb)
    color_sheet, sheet_w = _sheet(dac_big, nearest, w * 2, h * 2)
    scale_sheet, _ = _sheet(nearest, edge, w * 2, h * 2)
    img = {
        "01_dac_1x.png": (dac, w, h),
        "02_rgb888_1x.png": (rgb, w, h),
        "03_palette_diff_x64.png": (diff, w, h),
        "04_nearest_2x.png": (nearest, w * 2, h * 2),
        "05_edgeblend_2x.png": (edge, w * 2, h * 2),
        "06_palette_AB_same_frame.png": (color_sheet, sheet_w, h * 2),
        "07_upscale_AB_same_pixels.png": (scale_sheet, sheet_w, h * 2),
    }
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for filename, (pixels, width, height) in img.items():
        (destination / filename).write_bytes(encode_png(pixels, width, height))
    summary = {
        "type": "SHP asset/frame A/B, not a live game screenshot",
        "synthetic_demo": demo,
        "source_name": Path(source_name).name,
        "palette_name": Path(palette_name).name,
        "sha256_shp": hashlib.sha256(source).hexdigest(),
        "sha256_pal": hashlib.sha256(palette_bytes).hexdigest(),
        "sha256_indexed_frame": hashlib.sha256(indices).hexdigest(),
        "frame": frame, "frame_count": shape["total_frames"],
        "canvas": [shape["canvas_width"], shape["canvas_height"]],
        "cropped_frame": [shape["crop_x"], shape["crop_y"], w, h],
        "same_frame_indices_for_dac_rgb": True,
        "same_rgb888_pixels_for_nearest_edge": True,
        "two_x_output": [w * 2, h * 2],
        "color_stats": _palette_stats(dac, rgb),
        "scale_stats": {
            "different_2x_pixels": sum(a != b for a, b in zip(nearest, edge)),
            "total_2x_pixels": len(edge),
            "note": "Not an xBRZ implementation; no truth label for subjective sharpness.",
        },
        "image_order": list(img),
        "limitations": [
            "Raw file-backed SHP and one specified PAL; not VXL, TMP or full battlefield",
            "No dynamic player remap, CustomPalette, native LightConvert or scene lighting",
            "No game state access or game code modification; same SHP index buffer",
            "Color-range expansion cannot reconstruct detail absent from 256 source indices",
        ],
    }
    (destination / "AB_manifest.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
    return summary


def demo_files():
    width, height = 64, 48
    pal = bytearray(768)
    colors = {1: (63, 40, 4), 2: (12, 31, 63), 3: (63, 6, 10),
              4: (4, 62, 53), 5: (46, 46, 46), 6: (32, 13, 42)}
    for i, c in colors.items():
        pal[i * 3:i * 3 + 3] = bytes(c)
    frame = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            dx = (x - width / 2) / 27
            dy = (y - height / 2) / 18
            if dx * dx + dy * dy > 1:
                continue
            color = (3 if x + y > 74 else 2) if abs(x - y - 4) <= 2 else 1
            if y > 21 and abs(x - 31) < 13:
                color = 5 if (x // 5 + y // 5) % 2 else 4
            if x > 40 and y < 22:
                color = 6
            frame[y * width + x] = color
    data = bytearray(32 + len(frame))
    struct.pack_into("<4H", data, 0, 0, width, height, 1)
    struct.pack_into("<hhHHI", data, 8, 0, 0, width, height, 0)
    struct.pack_into("<I", data, 28, 32)
    data[32:] = frame
    return bytes(data), bytes(pal)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--shp", type=Path, help="locally extracted TS/YR SHP")
    parser.add_argument("--pal", type=Path, help="matching 768-byte RGB6 PAL")
    parser.add_argument("--frame", type=int, default=0)
    parser.add_argument("--demo", action="store_true",
                        help="copyright-free algorithm fixture; not real RA2 pixels")
    parser.add_argument("--out", type=Path, default=Path("palette-ab"))
    args = parser.parse_args(argv)
    if args.demo and (args.shp or args.pal):
        parser.error("--demo does not accept --shp / --pal")
    if args.demo:
        shp, pal = demo_files()
        names = ("synthetic_demo.shp", "synthetic_demo.pal")
    else:
        if not args.shp or not args.pal:
            parser.error("provide BOTH --shp and --pal, or use --demo")
        shp, pal = _load(args.shp), _load(args.pal)
        names = (str(args.shp), str(args.pal))
    summary = compare(shp, pal, args.frame, args.out, *names, demo=args.demo)
    print("PASS: same immutable SHP frame/palette across DAC/RGB888 and 2x methods")
    print("frame", summary["frame"], "crop", summary["cropped_frame"],
          "color difference", summary["color_stats"],
          "2x differing pixels", summary["scale_stats"]["different_2x_pixels"])
    print("files:", args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
