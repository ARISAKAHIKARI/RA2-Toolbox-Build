#!/usr/bin/env python3
"""Execute the production debug-entry INI and seqlock publishers portably."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def block(source, marker):
    begin = source.index(marker)
    opening = source.index("{", begin)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    source = tests.parent / "source"
    io = (source / "extra_settings_io.h").read_text()
    host = (source / "main.cpp").read_text()
    subject = block(host, "static void write(") + "\n" + "\n".join(block(io, name) for name in (
        "static void debugEntrySettingKey(", "static void loadDebugEntrySettings(",
        "static void saveDebugEntrySettings(", "static void syncDebugEntrySettings("))
    args.build_dir.mkdir(parents=True, exist_ok=True)
    (args.build_dir / "debug_entry_settings_subject.inc").write_text(subject)
    exe = args.build_dir / "debug-entry-settings"
    cmd = [args.compiler, "-std=c++17", "-O2", "-UNDEBUG", "-I" + str(args.build_dir),
           str(tests / "debug_entry_settings.cpp"), "-o", str(exe)]
    build = subprocess.run(cmd, capture_output=True, text=True)
    (args.build_dir / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    (args.build_dir / "run.log").write_text(run.stdout + run.stderr)
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    result = json.loads(run.stdout)
    result["subject_sha256"] = hashlib.sha256(subject.encode()).hexdigest()
    paths = [source / "additional_settings.h", source / "extra_settings_io.h", tests / "debug_entry_settings.cpp", Path(__file__),
             source.parents[1] / "shared/source/debug_entry_types.h", source.parents[1] / "shared/source/debug_entry_policy.h", source.parents[1] / "shared/source/debug_entry_catalog.h"]
    result["source_sha256"] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    (args.build_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result | {"source_sha256": "recorded"}))


if __name__ == "__main__":
    main()
