#!/usr/bin/env python3
"""Run production probe UI commands against controlled Win32 boundaries."""
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
    source = Path(__file__).resolve().parents[2]
    ui = source / "assist/source/platform_hook_probe_ui.h"
    # Only platform/collector declarations are replaced. All UI state,
    # command routes, worker publication and shutdown code run unmodified.
    subject = "\n".join(line for line in ui.read_text().splitlines()
                        if not line.startswith(("#include", "#pragma"))) + "\n"
    args.build_dir.mkdir(parents=True, exist_ok=True)
    (args.build_dir / "platform-hook-probe-ui-subject.inc").write_text(subject)
    exe = args.build_dir / "platform-hook-probe-ui"
    build = subprocess.run([args.compiler, "-std=c++17", "-O2", "-UNDEBUG",
                            "-Wall", "-Wextra", "-I" + str(args.build_dir),
                            str(Path(__file__).with_suffix(".cpp")), "-o", str(exe)],
                           capture_output=True, text=True)
    (args.build_dir / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    (args.build_dir / "run.log").write_text(run.stdout + run.stderr)
    if run.returncode:
        raise SystemExit(run.stdout + run.stderr)
    result = json.loads(run.stdout)
    paths = [ui, Path(__file__), Path(__file__).with_suffix(".cpp"),
             source / "assist/source/debug_entries_ui.h", source / "assist/source/main.cpp",
             source / "suite/frontend/app.js", source / "suite/frontend/app.css"]
    paths.append(source / "external_reader/source/game_process_names.h")
    result["source_sha256"] = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in paths}
    result["subject_sha256"] = hashlib.sha256(subject.encode()).hexdigest()
    (args.build_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result | {"source_sha256": "recorded"}))


if __name__ == "__main__":
    main()
