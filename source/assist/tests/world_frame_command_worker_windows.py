"""Build and run actual x86/x64 pinned command CaptureGraph Win32 workers."""
from pathlib import Path
import argparse
import subprocess
import hashlib
import json

parser = argparse.ArgumentParser()
parser.add_argument("--zig", required=True, type=Path)
parser.add_argument("--out", required=True, type=Path)
args = parser.parse_args()
args.out = args.out.resolve()
args.out.mkdir(parents=True, exist_ok=True)
here = Path(__file__).resolve().parent
runs = []
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
    for iteration in range(3):
        completed = subprocess.run([str(exe), str(dll)], text=True,
                                   capture_output=True, timeout=35)
        (dest / f"run-{iteration}.log").write_text(completed.stdout + completed.stderr)
        print(arch, iteration, completed.returncode, completed.stdout, completed.stderr)
        runs.append({"arch": arch, "iteration": iteration,
                     "exit": completed.returncode, "pass": completed.returncode == 0})
        if completed.returncode:
            break
passed = len(runs) == 6 and all(item["pass"] for item in runs)
src = [here / "world_frame_command_worker_dll.cpp",
       here / "world_frame_command_worker_host.cpp",
       here.parent.parent / "shared/source/world_frame_command_worker_win.h",
       here.parent.parent / "shared/source/world_frame_command_policy.h"]
(args.out / "result.json").write_text(json.dumps({
    "pass": passed, "runs": runs, "native_windows": True,
    "real_game": False, "multiplayer": False,
    "source_sha256": {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in src}
}, indent=2) + "\n")
assert passed
