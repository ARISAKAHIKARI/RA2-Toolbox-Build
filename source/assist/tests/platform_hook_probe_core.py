#!/usr/bin/env python3
"""Build and run the production PE core against bounded synthetic fixtures.

Both an optimized host build and one ASan/UBSan build are run. This is portable
parser/comparator evidence, not a Windows process or real-game integration run.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="analysis/platform-hook-probe-core")
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    out = Path(args.out)
    if not out.is_absolute():
        out = root / out
    out.mkdir(parents=True, exist_ok=True)
    paths = [
        "source/shared/source/platform_hook_probe_core.h",
        "source/assist/tests/platform_hook_probe_core.cpp",
        "source/assist/tests/platform_hook_probe_core.py",
    ]
    before = {path: sha(root / path) for path in paths}
    commands = []
    runs = []

    def run(argv, timeout=90, environment=None):
        started = time.monotonic()
        result = subprocess.run(argv, cwd=root, capture_output=True, text=True,
                                timeout=timeout, env=environment)
        commands.append({"argv": [str(item) for item in argv],
                         "exit_code": result.returncode,
                         "seconds": round(time.monotonic() - started, 3),
                         "stdout": result.stdout, "stderr": result.stderr})
        return result

    common = [args.compiler, "-std=c++17", "-UNDEBUG", "-Wall", "-Wextra",
              "-Wpedantic", "-Werror", paths[1]]
    for name, flags in [
        ("optimized", ["-O2"]),
        ("asan-ubsan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]),
    ]:
        executable = out / ("platform_hook_probe_core_" + name)
        built = run(common + flags + ["-o", str(executable)])
        if built.returncode:
            break
        # This container restricts LeakSanitizer's /proc task inspection. Address
        # and UB instrumentation remain enabled; leak coverage is not claimed.
        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
                           UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        executed = run([str(executable)], timeout=30, environment=environment)
        if executed.returncode:
            break
        report = json.loads(executed.stdout)
        runs.append({"name": name, "executable_sha256": sha(executable), **report})

    stable = before == {path: sha(root / path) for path in paths}
    passed = (stable and len(runs) == 2 and all(item["passed"] for item in runs)
              and all(command["exit_code"] == 0 for command in commands))
    receipt = {
        "passed": passed,
        "source_sha256": before,
        "source_hash_stable": stable,
        "evidence_scope": "production portable PE parser/comparator with synthetic PE32/PE32+ fixtures",
        "game_process_executed": False,
        "windows_process_executed": False,
        "platform_injection_executed": False,
        "baseline_authenticity_proven": False,
        "leak_sanitizer_executed": False,
        "leak_sanitizer_limitation": "container /proc task inspection unavailable; ASan and UBSan remain enabled",
        "runs": runs,
        "commands": commands,
    }
    receipt_path = out / "result.json"
    receipt_path.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({"passed": passed, "receipt": str(receipt_path), "runs": runs}, ensure_ascii=False))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
