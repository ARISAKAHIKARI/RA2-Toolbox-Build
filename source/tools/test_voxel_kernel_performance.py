"""Build changing-pose production VXL kernels against explicit old/current headers.

The baseline directory must contain voxel_preview.h and gpu_voxel.h copied from
the comparison revision. Other (unchanged) dependencies use the current tree.
No game memory, complete scene/pose cache, GPU execution or live FPS is involved.
"""
import argparse
import json
from pathlib import Path
import shutil
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--zig", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    tests, headers = source / "assist/tests", source / "external_reader/source"
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    old = out / "baseline"
    old.mkdir(exist_ok=True)
    for name in ("voxel_preview.h", "gpu_voxel.h"):
        shutil.copyfile(args.baseline / name, old / name)
    shutil.copyfile(tests / "voxel_dynamic_cost.cpp", old / "voxel_dynamic_cost.cpp")
    cases = (tests / "gpu_voxel_cases.h").read_text(encoding="utf-8")
    (old / "gpu_voxel_cases.h").write_text(
        cases.replace('../../external_reader/source/gpu_voxel.h', 'gpu_voxel.h'), encoding="utf-8")

    def build(test, executable, target="x86_64-windows-gnu"):
        cmd = [str(args.zig.resolve()), "c++", "-target", target, "-std=c++17", "-O2", "-UNDEBUG",
               "-I", str(headers), str(test), "-o", str(executable)]
        subprocess.run(cmd, check=True)

    def run(executable):
        result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
        return result.stdout.strip()

    before, after = out / "before.exe", out / "after.exe"
    build(old / "voxel_dynamic_cost.cpp", before)
    build(tests / "voxel_dynamic_cost.cpp", after)
    runs = {"before": [], "after": []}
    for trial in range(5):
        for name in (("before", "after") if trial % 2 == 0 else ("after", "before")):
            result = json.loads(run(before if name == "before" else after))
            assert result["status"] == "PASS"
            runs[name].append(result)
    digests = {row["digest"] for group in runs.values() for row in group}
    assert len(digests) == 1, f"Baseline/current voxel pixels, depth, indices or GPU triangles differ: {digests}"
    timings = {}
    for key in ("median_hva_ms", "median_gpu_prepare_ms", "median_cpu_raster_ms", "median_shadow_ms"):
        a = statistics.median(row[key] for row in runs["before"])
        b = statistics.median(row[key] for row in runs["after"])
        timings[key] = {"before": a, "after": b, "ratio": b / a}
    equivalence = {}
    for target in ("x86_64-windows-gnu", "x86-windows-gnu"):
        exe = out / f"equivalence-{target}.exe"
        build(tests / "voxel_kernel_equivalence.cpp", exe, target)
        equivalence[target] = run(exe)
    result = {"status": "PASS", "digest": next(iter(digests)), "timings": timings,
              "equivalence": equivalence, "runs": runs,
              "scope": "synthetic exposed VXL geometry; changing unit poses; actual production CPU kernels/GPU preparation; no GPU execution or live FPS"}
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in result.items() if key != "runs"}, indent=2))


if __name__ == "__main__":
    main()
