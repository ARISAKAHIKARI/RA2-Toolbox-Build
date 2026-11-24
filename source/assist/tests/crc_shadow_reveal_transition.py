#!/usr/bin/env python3
"""Run the production single-store scope policy as a real Windows x86 executable.

The separate RuntimeScopedCall Windows fixture validates the actual finally
trampoline. This fixture validates target-House matching, one-shot skip,
identity-change fail-open, abort and scope cleanup without game memory.
"""
import argparse
from pathlib import Path
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    source = tests / "crc_shadow_single_store_feasibility.cpp"
    args.build_dir.mkdir(parents=True, exist_ok=True)
    exe = args.build_dir / "crc_shadow_single_store_policy_x86.exe"
    subprocess.run([
        str(args.zig.resolve()), "c++", "-target", "x86-windows-gnu",
        "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
        str(source.resolve()), "-o", str(exe.resolve())
    ], check=True)
    subprocess.run([str(exe.resolve())], check=True)


if __name__ == "__main__":
    main()
