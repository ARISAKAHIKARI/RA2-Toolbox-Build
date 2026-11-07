#!/usr/bin/env python3
"""Run the production process-selection UI with controlled Win32/API boundaries."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    source = here.parent / "source/game_process_selection_ui.h"
    before = source.read_bytes()
    # The production UI is unchanged. Only Win32 and the separately tested
    # policy/persistence API are replaced by deterministic boundary fixtures.
    subject = "\n".join(line for line in before.decode().splitlines()
                        if not line.startswith(("#include", "#pragma"))) + "\n"
    args.build_dir.mkdir(parents=True, exist_ok=True)
    (args.build_dir / "game-process-selection-ui-subject.inc").write_text(subject)
    executable = args.build_dir / "game-process-selection-ui"
    build = subprocess.run([
        args.compiler, "-std=c++17", "-O2", "-UNDEBUG", "-Wall", "-Wextra",
        "-I" + str(args.build_dir), str(here / "game_process_selection_ui.cpp"),
        "-o", str(executable)], capture_output=True, text=True)
    (args.build_dir / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit(build.stdout + build.stderr)
    run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=20)
    (args.build_dir / "run.log").write_text(run.stdout + run.stderr)
    if run.returncode:
        raise SystemExit(run.stdout + run.stderr)
    assert source.read_bytes() == before, "Production UI changed during test"
    result = json.loads(run.stdout)
    result["production_source_sha256"] = hashlib.sha256(before).hexdigest()
    result["subject_sha256"] = hashlib.sha256(subject.encode()).hexdigest()
    result["fixture_sha256"] = hashlib.sha256((here / "game_process_selection_ui.cpp").read_bytes()).hexdigest()
    (args.build_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
