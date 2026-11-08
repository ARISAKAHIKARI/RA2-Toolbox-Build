"""Measure exact servicePlanner capture before/after bounded World reuse.

Portable CPU/allocation fixture with explicit UI and worker seams, not Windows,
game execution, or gameplay FPS. The real planner Task verifies retained ownership.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile
from tactical_frame_sampling import function

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
    out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="planner-world-pool-"))
    out.mkdir(parents=True, exist_ok=True)
    old = subprocess.check_output(["git", "show", f"{args.baseline}:source/assist/source/main.cpp"], cwd=ROOT).decode()
    current = (SRC / "main.cpp").read_text()
    results = {}
    flags = ["-std=c++17", "-O2", "-UNDEBUG"]
    for name, source, before in (("before", old, True), ("after", current, False)):
        work = out / name
        work.mkdir(exist_ok=True)
        generated = function(source, "static void servicePlanner(")
        (work / "planner_service_production.h").write_text(generated + "\n")
        exe = work / "planner-world-pool"
        command = [args.cxx, *flags, f"-DPLANNER_POOL_BASELINE={int(before)}", "-I", str(work), "-I", str(SRC), str(HERE / "planner_world_pool.cpp"), "-o", str(exe)]
        build = subprocess.run(command, capture_output=True, text=True)
        (work / "build.log").write_text(build.stdout + build.stderr)
        print(build.stdout + build.stderr, end="")
        build.check_returncode()
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
        (work / "run.log").write_text(run.stdout + run.stderr)
        print(name + ":\n" + run.stdout + run.stderr, end="")
        run.check_returncode()
        metrics = [json.loads(line.removeprefix("METRIC ")) for line in run.stdout.splitlines() if line.startswith("METRIC ")]
        if len(metrics) != 1:
            raise RuntimeError("missing service measurement")
        results[name] = {"metrics": metrics[0], "generated_sha256": sha(generated.encode()), "executable_sha256": sha(exe.read_bytes()), "command": command}
    before, after = results["before"]["metrics"], results["after"]["metrics"]
    if before["final_active_digest"] != after["final_active_digest"]:
        raise RuntimeError("active service outputs differ between revisions")
    paths = [SRC / "main.cpp", SRC / "planner_world_pool.h", SRC / "planner_task.h", SRC / "planner_types.h", SRC / "external_planner.h", SRC / "plan_freshness.h", HERE / "planner_world_pool.cpp", Path(__file__).resolve(), HERE / "tactical_frame_sampling.py"]
    report = {"pass": True, "scope": __doc__, "baseline_commit": args.baseline, "compiler": subprocess.check_output([args.cxx, "--version"], text=True).splitlines()[0], "results": results,
              "windows_executed": False, "game_process_tested": False, "fps_measured": False,
              "source_sha256": {str(p.relative_to(ROOT)): sha(p.read_bytes()) for p in paths},
              "median_elapsed_ratio_after_over_before": after["median_batch_ns"] / before["median_batch_ns"]}
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
