"""Compile the actual Host needsBridge expression, including an optional baseline."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--zig", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--baseline-main", type=Path)
    args = p.parse_args()
    source = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    results = {}
    variants = [("current", source / "assist/source/main.cpp")]
    if args.baseline_main:
        variants.insert(0, ("before", args.baseline_main))
    for name, host in variants:
        expression = re.search(r"\bbool needsBridge=[^;]+;", host.read_text(encoding="utf-8"))
        assert expression, f"No real Host needsBridge declaration in {host}"
        include = out / f"{name}-callsite.h"
        include.write_text(expression.group() + "\n", encoding="utf-8")
        for target in ("x86_64-windows-gnu", "x86-windows-gnu"):
            exe = out / f"{name}-{target}.exe"
            subprocess.run([str(args.zig.resolve()), "c++", "-target", target, "-std=c++17", "-O2",
                            '-DRA2_RUNTIME_DEMAND_CALLSITE="' + include.as_posix() + '"',
                            str(source / "assist/tests/runtime_feature_demand.cpp"), "-o", str(exe)], check=True)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            results[f"{name}-{target}"] = {"exit": result.returncode, "output": result.stdout.strip()}
            if name == "current":
                assert result.returncode == 0, result.stdout + result.stderr
            else:
                assert result.returncode == 1 and "FAIL 121 " in result.stdout, "Baseline did not reproduce missing cold demands"
    (out / "result.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
