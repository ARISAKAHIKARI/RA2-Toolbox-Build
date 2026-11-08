"""Compare exact baseline/current planner publication on a sealed synthetic world.

Linux CPU/memcpy observations only; no Windows, game-process or FPS measurement.
Both binaries use unchanged production function bodies and the same memory seams.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile
import tactical_frame_sampling as tactical

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "source"
ROOT = HERE.parents[2]
BASELINE = "88812eda0d23d065d61d6f742315b7d0581fd1a8"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--baseline", default=BASELINE)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="planner-publication-"))
    out.mkdir(parents=True, exist_ok=True)
    current, scope, names = tactical.production()
    path = "source/assist/source/external_orders_bridge.h"
    old = subprocess.check_output(["git", "show", f"{args.baseline}:{path}"], cwd=ROOT).decode()
    now = (SRC / "external_orders_bridge.h").read_text()
    signature = "static bool publishPlanningWorld("
    old_function, new_function = tactical.function(old, signature), tactical.function(now, signature)
    if current.count(new_function) != 1:
        raise RuntimeError("production publisher extraction is not unique")
    results = {}
    flags = ["-std=c++17", "-O2", "-UNDEBUG", "-fno-builtin-memcpy", "-Wl,--wrap=memcpy"]
    for name, before in (("before", True), ("after", False)):
        work = out / name
        work.mkdir(exist_ok=True)
        generated = current.replace(new_function, old_function) if before else current
        (work / "tactical_frame_production.h").write_text(generated)
        (work / "sample_scope_production.h").write_text(scope)
        exe = work / "planner-publication"
        command = [args.cxx, *flags, f"-DPLANNER_PUBLICATION_BASELINE={int(before)}", "-I", str(work), "-I", str(SRC), str(HERE / "planner_publication_performance.cpp"), "-o", str(exe)]
        build = subprocess.run(command, capture_output=True, text=True)
        (work / "build.log").write_text(build.stdout + build.stderr)
        print(build.stdout + build.stderr, end="")
        build.check_returncode()
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
        (work / "run.log").write_text(run.stdout + run.stderr)
        print(name + ":\n" + run.stdout + run.stderr, end="")
        run.check_returncode()
        metric = [json.loads(line.removeprefix("METRIC ")) for line in run.stdout.splitlines() if line.startswith("METRIC ")]
        if len(metric) != 1:
            raise RuntimeError("missing publication measurement")
        results[name] = {"metrics": metric[0], "generated_sha256": sha(generated.encode()), "executable_sha256": sha(exe.read_bytes()), "command": command}
    before, after = results["before"]["metrics"], results["after"]["metrics"]
    if before["final_active_digest"] != after["final_active_digest"]:
        raise RuntimeError("active production outputs differ between revisions")
    paths = [SRC / n for n in names] + [SRC / "planner_world_copy.h", SRC / "planner_types.h", SRC / "external_planner.h", SRC / "tactical_frame_read.h", SRC / "raw_combat.h", SRC / "combat_probe_bridge.h", SRC / "ore_harvest_bridge.h", SRC.parents[1] / "shared/source/world_frame.h", HERE / "tactical_frame_sampling.cpp", HERE / "tactical_frame_sampling.py", HERE / "planner_publication_performance.cpp", Path(__file__).resolve()]
    report = {"pass": True, "scope": __doc__, "baseline_commit": args.baseline, "baseline_file_sha256": sha(old.encode()), "baseline_function_sha256": sha(old_function.encode()), "current_function_sha256": sha(new_function.encode()), "compiler": subprocess.check_output([args.cxx, "--version"], text=True).splitlines()[0], "results": results,
              "windows_executed": False, "game_process_tested": False, "fps_measured": False,
              "source_sha256": {str(p.relative_to(ROOT)): sha(p.read_bytes()) for p in paths},
              "median_elapsed_ratio_after_over_before": after["median_batch_ns"] / before["median_batch_ns"]}
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
