"""Verify synchronous Runtime default and opt-in x86/x64 command CaptureGraph workers."""
from pathlib import Path
import argparse
import subprocess
import hashlib
import json
import os

parser = argparse.ArgumentParser()
parser.add_argument("--zig", required=True, type=Path)
parser.add_argument("--out", required=True, type=Path)
args = parser.parse_args()
args.out = args.out.resolve()
args.out.mkdir(parents=True, exist_ok=True)
here = Path(__file__).resolve().parent
runs = []
benchmarks = []
graph_benchmarks = []
for arch in ("x86", "x86_64"):
    dest = args.out / arch
    dest.mkdir(exist_ok=True)
    dll, exe = dest / "command-worker.dll", dest / "command-worker-host.exe"
    for source, target, extra in (
        ("world_frame_command_worker_dll.cpp", dll, ["-shared"]),
        ("world_frame_command_worker_host.cpp", exe, []),
    ):
        argv = [str(args.zig), "c++", "-target", arch + "-windows-gnu", "-std=c++17",
                "-O2", "-UNDEBUG", *extra, str(here / source), "-o", str(target)]
        completed = subprocess.run(argv, text=True, capture_output=True)
        (dest / (source + ".build.log")).write_text(completed.stdout + completed.stderr)
        if completed.returncode:
            print(completed.stdout + completed.stderr)
            raise SystemExit(completed.returncode)
    # The product default is synchronous. Only explicit mode "1" enables
    # this worker for isolated benchmarking and lifecycle regression.
    for mode in ("default", "0", "1"):
        for iteration in range(2):
            env = os.environ.copy()
            env.pop("RA2TOOLBOX_ASYNC_COMMAND_CAPTURE", None)
            if mode != "default":
                env["RA2TOOLBOX_ASYNC_COMMAND_CAPTURE"] = mode
            expected = "1" if mode == "1" else "0"
            completed = subprocess.run([str(exe), str(dll), expected], env=env,
                                       text=True, capture_output=True, timeout=35)
            (dest / f"run-{mode}-{iteration}.log").write_text(
                completed.stdout + completed.stderr)
            print(arch, mode, iteration, completed.returncode,
                  completed.stdout, completed.stderr)
            runs.append({"arch": arch, "mode": mode, "iteration": iteration,
                         "exit": completed.returncode,
                         "pass": completed.returncode == 0})
            if completed.returncode:
                break
    # Synthetic native benchmark; its timings are logged, never a
    # pass/fail performance threshold on noisy hosted CI machines.
    benchmark_source = here / "world_frame_copy_microbench.cpp"
    benchmark_exe = dest / "world-frame-copy-bench.exe"
    build = subprocess.run([
        str(args.zig), "c++", "-target", arch + "-windows-gnu",
        "-std=c++17", "-O2", "-UNDEBUG", str(benchmark_source),
        "-o", str(benchmark_exe)
    ], text=True, capture_output=True)
    (dest / "copy-bench.build.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        print(build.stdout + build.stderr)
        raise SystemExit(build.returncode)
    result = subprocess.run([str(benchmark_exe)], text=True,
                            capture_output=True, timeout=40)
    (dest / "copy-bench.run.log").write_text(result.stdout + result.stderr)
    print(arch, "copy-bench", result.returncode, result.stdout, result.stderr)
    benchmarks.append({"arch": arch, "exit": result.returncode,
                       "pass": result.returncode == 0,
                       "result": result.stdout.strip(),
                       "synthetic": True, "real_game": False})
    # Full CaptureGraph benchmark: same graph/index, different same-process readers.
    graph_source = here / "world_frame_command_worker_graph_benchmark.cpp"
    graph_exe = dest / "world-frame-graph-reader-bench.exe"
    build = subprocess.run([
        str(args.zig), "c++", "-target", arch + "-windows-gnu",
        "-std=c++17", "-O2", "-UNDEBUG", str(graph_source),
        "-o", str(graph_exe)
    ], text=True, capture_output=True)
    (dest / "graph-bench.build.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        print(build.stdout + build.stderr)
        raise SystemExit(build.returncode)
    result = subprocess.run([str(graph_exe)], text=True,
                            capture_output=True, timeout=90)
    (dest / "graph-bench.run.log").write_text(result.stdout + result.stderr)
    print(arch, "graph-bench", result.returncode, result.stdout, result.stderr)
    graph_benchmarks.append({"arch": arch, "exit": result.returncode,
                             "pass": result.returncode == 0,
                             "result": result.stdout.strip(),
                             "synthetic": True, "real_game": False})
passed = (len(runs) == 12 and all(item["pass"] for item in runs)
          and len(benchmarks) == 2 and all(item["pass"] for item in benchmarks)
          and len(graph_benchmarks) == 2 and all(item["pass"] for item in graph_benchmarks))
src = [here / "world_frame_command_worker_windows.py",
       here / "world_frame_command_worker_dll.cpp",
       here / "world_frame_command_worker_host.cpp",
       here / "world_frame_copy_microbench.cpp",
       here / "world_frame_command_worker_graph_benchmark.cpp",
       here.parent.parent / "shared/source/world_frame_command_worker_win.h",
       here.parent.parent / "shared/source/world_frame_command_policy.h",
       here.parent.parent / "shared/source/world_frame_capture.h",
       here.parent.parent / "shared/source/world_frame_command_index.h",
       here.parent.parent / "shared/source/world_frame.h",
       here.parent.parent / "assist/source/object_index.h"]
(args.out / "result.json").write_text(json.dumps({
    "pass": passed, "runs": runs, "benchmarks": benchmarks,
    "graph_benchmarks": graph_benchmarks, "native_windows": True,
    "real_game": False, "multiplayer": False,
    "source_sha256": {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in src}
}, indent=2) + "\n")
assert passed
