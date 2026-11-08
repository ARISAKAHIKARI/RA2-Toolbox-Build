"""Count real reverse-dodge collector work with explicit native/Win32 seams.

The routing, shot table, target collector, continuing-job input collector and
reverse vector code are extracted without rewriting their bodies. Synthetic
memory and the final native-order actor are controlled boundaries. This is not
Windows, real projectiles, netplay or an FPS benchmark.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
SRC = HERE.parent / "source"


def function(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def extract(reverse, dodge):
    parts = [reverse[reverse.index("struct ReverseShot"):reverse.index("static bool reverseVector(")],
             function(reverse, "static bool reverseVector("),
             function(dodge, "static bool dodgeThreatCurrent("),
             reverse[reverse.index("struct ReverseTarget"):reverse.index("static bool reverseDodgeTick(")],
             function(reverse, "static bool reverseDodgeTick(")]
    return "\n\n".join(parts) + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--baseline-ref", default="a4450ee784ad7844cacf848f1319c55e10aa8c23")
    p.add_argument("--baseline-only", action="store_true")
    args = p.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    relative = "source/assist/source/reverse_dodge_bridge.h"
    baseline = subprocess.check_output(["git", "show", args.baseline_ref + ":" + relative], cwd=REPO).decode()
    dodge = (SRC / "dodge_bridge.h").read_text()
    variants = {"before": baseline}
    if not args.baseline_only:
        variants["after"] = (SRC / "reverse_dodge_bridge.h").read_text()
    records = {}
    for label, reverse in variants.items():
        directory = out / label; directory.mkdir(exist_ok=True)
        header = directory / "reverse_hotpath_production.h"
        content = extract(reverse, dodge); header.write_text(content)
        exe = directory / "reverse-hotpath"
        command = ["g++", "-std=c++17", "-O2", "-UNDEBUG", "-I", str(directory), "-I", str(SRC),
                   str(HERE / "reverse_dodge_hotpath.cpp"), "-o", str(exe)]
        build = subprocess.run(command, capture_output=True, text=True)
        (directory / "build.log").write_text(build.stdout + build.stderr)
        build.check_returncode()
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        (directory / "run.log").write_text(run.stdout + run.stderr)
        if run.returncode:
            print(run.stdout + run.stderr)
        run.check_returncode()
        result = json.loads(run.stdout)
        records[label] = {"command": command, "run_command": [str(exe)], "exit_code": 0, "result": result,
                          "extracted_sha256": hashlib.sha256(content.encode()).hexdigest(),
                          "reverse_source_sha256": hashlib.sha256(reverse.encode()).hexdigest(),
                          "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest()}
    if "after" in records:
        before, after = (records[k]["result"] for k in ("before", "after"))
        assert before["semantic_digest"] == after["semantic_digest"]
        assert before["cases"] == after["cases"] and before["continuation_passes"] == after["continuation_passes"]
        assert before["stress_actor_attempts"] == after["stress_actor_attempts"] == 64
        assert before["repeated_roster_visits"] == 64 * 512 * 99
        assert after["repeated_roster_visits"] == 0
        assert after["first_roster_visits"] == before["first_roster_visits"] == 64 * 512
    paths = [SRC / "reverse_dodge_bridge.h", SRC / "dodge_bridge.h", SRC / "reverse_dodge.h",
             SRC / "dodge_compute.h", HERE / "reverse_dodge_hotpath.cpp", Path(__file__).resolve()]
    receipt = {"pass": True, "scope": __doc__, "baseline_commit": args.baseline_ref,
               "baseline_only": args.baseline_only, "variants": records,
               "source_sha256": {str(f.relative_to(REPO)): hashlib.sha256(f.read_bytes()).hexdigest() for f in paths},
               "windows_executed": False, "game_tested": False, "multiplayer_tested": False}
    (out / "result.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({k:v["result"] for k,v in records.items()}, indent=2))


if __name__ == "__main__":
    main()
