"""Measure the privately supplied 920x564 preview/native comparison screenshot.

This is an optional, image-specific research analysis, not a live-game test.
The input image is never copied or emitted. Output contains only its SHA-256,
alignment scores, aggregate channel statistics, and a few RGB sample values.
Requires Pillow, NumPy, and SciPy. The fixed regions/search bounds describe the
2026-10-25 comparison composition; low-confidence alignment rejects statistics.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter


REFERENCE_SHA256 = "f0580947799b630f14cbf5ba775ef95f35d8188a8bf2167f2a94b413ff972539"
TOP_REGIONS = {
    "power": (42, 82, 109, 139),
    "radar": (116, 102, 170, 152),
    "factory": (127, 144, 217, 208),
    "barracks": (44, 168, 113, 224),
}
BOTTOM_REGION = (45, 304, 145, 360)


def coordinates(box):
    x0, y0, x1, y1 = box
    return np.mgrid[y0:y1, x0:x1]


def high_pass(rgb):
    gray = rgb.mean(axis=2)
    return gray - gaussian_filter(gray, 1.3)


def align(image, box, x_offsets, y_offsets):
    yy, xx = coordinates(box)
    target = high_pass(image[yy, xx]).ravel()
    target = target - target.mean()
    target_norm = np.linalg.norm(target)
    candidates = []
    for dx in x_offsets:
        for dy in y_offsets:
            sx, sy = 2 * xx + dx, 2 * yy + dy
            if (sx.min() < 0 or sy.min() < 0 or sx.max() >= image.shape[1]
                    or sy.max() >= image.shape[0]):
                continue
            source = high_pass(image[sy, sx]).ravel()
            source = source - source.mean()
            denominator = target_norm * np.linalg.norm(source)
            score = float(np.dot(target, source) / denominator) if denominator else -1.0
            candidates.append((score, dx, dy))
    if len(candidates) < 2:
        raise ValueError("The image does not contain the required comparison regions")
    candidates.sort(reverse=True)
    best, second = candidates[:2]
    return {
        "region_xyxy_exclusive": list(box),
        "native_x": {"watch_multiplier": 2, "offset": best[1]},
        "native_y": {"watch_multiplier": 2, "offset": best[2]},
        "high_pass_correlation": best[0],
        "next_candidate_correlation": second[0],
        "correlation_margin": best[0] - second[0],
        "accepted": bool(best[0] >= 0.90 and best[0] - second[0] >= 0.15),
    }


def offsets(alignment):
    return alignment["native_x"]["offset"], alignment["native_y"]["offset"]


def channel_statistics(image, top_alignments):
    dx, dy = offsets(next(iter(top_alignments.values())))
    points = []
    repeated_colors = []
    for name, box in TOP_REGIONS.items():
        yy, xx = coordinates(box)
        native, watch = image[2 * yy + dy, 2 * xx + dx], image[yy, xx]
        # Selection uses only native colors, never the preview's hue/brightness.
        neutral = (native.min(axis=2) >= 160) & (np.ptp(native, axis=2) <= 8)
        points.extend(zip(xx[neutral].tolist(), yy[neutral].tolist()))
        for rgb in ((222, 223, 222), (214, 215, 214)):
            same_native = np.all(native == rgb, axis=2)
            counts = Counter(map(tuple, watch[same_native].astype(int).tolist()))
            repeated_colors.append({
                "region": name,
                "native_rgb": list(rgb),
                "preview_modes": [
                    {"rgb": list(color), "count": count}
                    for color, count in counts.most_common(3)
                ],
            })
    unique = np.asarray(sorted(set(points)), dtype=int)
    if len(unique) == 0:
        raise ValueError("No native neutral bright pixels exist in the aligned regions")
    native = image[2 * unique[:, 1] + dy, 2 * unique[:, 0] + dx]
    watch = image[unique[:, 1], unique[:, 0]]
    ratio = watch / native
    examples = []
    for name, x, y in (("factory", 184, 185), ("barracks", 81, 195)):
        sx, sy = 2 * x + dx, 2 * y + dy
        examples.append({
            "region": name, "watch_xy": [x, y], "native_xy": [sx, sy],
            "watch_rgb": image[y, x].astype(int).tolist(),
            "native_rgb": image[sy, sx].astype(int).tolist(),
        })
    return {
        "selection": "native min(R,G,B) >= 160 and max(R,G,B)-min(R,G,B) <= 8",
        "samples_before_region_overlap_deduplication": len(points),
        "unique_pixels": len(unique),
        "channel_order": "RGB",
        "preview_divided_by_native_median": np.median(ratio, axis=0).tolist(),
        "preview_divided_by_native_quartiles": np.quantile(ratio, [.25, .75], axis=0).tolist(),
        "repeated_native_colors": repeated_colors,
        "sample_pixels_zero_based": examples,
    }


def analyze(path):
    source_hash = hashlib.sha256(path.read_bytes()).hexdigest()
    with Image.open(path) as source:
        if source.size != (920, 564):
            raise ValueError("This analysis requires the original 920x564 comparison composition")
        image = np.asarray(source.convert("RGB"), dtype=np.float64)
    top = {
        name: align(image, box, range(274, 291), range(-72, -49))
        for name, box in TOP_REGIONS.items()
    }
    bottom = align(image, BOTTOM_REGION, range(426, 461), range(-410, -349))
    top_ok = all(row["accepted"] for row in top.values())
    top_ok = top_ok and len({offsets(row) for row in top.values()}) == 1
    result = {
        "scope": "Screenshot diagnosis only; no post-fix live-game validation",
        "image_sha256": source_hash,
        "is_original_reference_image": source_hash == REFERENCE_SHA256,
        "image_size": [920, 564],
        "screenshot_build_version": "unknown",
        "alignment_method": "2:1 integer nearest-sample search; Pearson correlation after Gaussian sigma=1.3 high-pass",
        "top_windows_alignment_accepted": top_ok,
        "top_regions": top,
        "bottom_factory": bottom,
    }
    if top_ok:
        result["neutral_bright_statistics"] = channel_statistics(image, top)
    if top_ok and bottom["accepted"]:
        tx, ty = offsets(next(iter(top.values())))
        bx, by = offsets(bottom)
        if (bx - tx) % 2 == 0 and (by - ty) % 2 == 0:
            yy, xx = coordinates(BOTTOM_REGION)
            yy_top, xx_top = yy + (by - ty) // 2, xx + (bx - tx) // 2
            first, second = image[yy, xx], image[yy_top, xx_top]
            result["factory_cross_window"] = {
                "top_x_minus_bottom_x": (bx - tx) // 2,
                "top_y_minus_bottom_y": (by - ty) // 2,
                "total_pixels": int(xx.size),
                "exact_rgb_matches": int(np.all(first == second, axis=2).sum()),
                "mean_absolute_channel_difference": float(np.abs(first - second).mean()),
            }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="Private original comparison image")
    parser.add_argument("--output", type=Path, help="Optional sanitized JSON destination")
    args = parser.parse_args()
    try:
        result = analyze(args.image)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    output = json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
