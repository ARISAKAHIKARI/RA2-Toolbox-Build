"""Build and run actual x86/x64 pinned command CaptureGraph Win32 workers."""
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
    # Test the real native policy in fresh processes: absent => enabled,
    # explicit 0 => disabled, explicit 1 => enabled.
    for mode in ("default", "0", "1"):
        for iteration in range(2):
            env = os.environ.copy()
            env.pop("RA2TOOLBOX_ASYNC_COMMAND_CAPTURE", None)
            if mode != "default":
                env["RA2TOOLBOX_ASYNC_COMMAND_CAPTURE"] = mode
            expected = "0" if mode == "0" else "1"
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
passed = len(runs) == 12 and all(item["pass"] for item in runs)
src = [here / "world_frame_command_worker_windows.py",
       here / "world_frame_command_worker_dll.cpp",
       here / "world_frame_command_worker_host.cpp",
       here.parent.parent / "shared/source/world_frame_command_worker_win.h",
       here.parent.parent / "shared/source/world_frame_command_policy.h"]
(args.out / "result.json").write_text(json.dumps({
    "pass": passed, "runs": runs, "native_windows": True,
    "real_game": False, "multiplayer": False,
    "source_sha256": {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in src}
}, indent=2) + "\n")
assert passed
