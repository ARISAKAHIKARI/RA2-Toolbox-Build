"""Compare actual identity routing with explicit live/guarded/sealed memory boundaries."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
from entry_hotpath import function

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SRC = HERE.parent / "source"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", default="061e106b71c81d79ea687edf36ad8401ffb722f8")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    path = "source/assist/source/external_orders_bridge.h"
    before = subprocess.check_output(["git", "show", args.baseline + ":" + path], cwd=ROOT).decode()
    after = (ROOT / path).read_text()
    results = {}
    for label, source in [("before", before), ("after", after)]:
        directory = out / label; directory.mkdir(exist_ok=True)
        body = (function(source, "static bool readObjectIdentity(") + "\n") if label == "after" else ""
        body += "\n".join(function(source, signature) for signature in ["static uint32_t worldByID(", "static uint32_t rawObjectID("])
        (directory / "identity_body.inc").write_text(body)
        exe = directory / "object-identity"
        command = ["g++", "-std=c++17", "-O2", "-UNDEBUG", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(directory), "-I" + str(SRC), str(HERE / "object_identity_read.cpp"), "-o", str(exe)]
        if label == "after":
            command.insert(1, "-DLOCAL_IDENTITY=1")
        build = subprocess.run(command, capture_output=True, text=True)
        (directory / "build.log").write_text(build.stdout + build.stderr)
        if build.returncode:
            raise RuntimeError(build.stdout + build.stderr)
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        (directory / "run.log").write_text(run.stdout + run.stderr)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        results[label] = json.loads(run.stdout)
    assert results["before"]["semantic_digest"] == results["after"]["semantic_digest"]
    paths = [ROOT / path, SRC / "object_index.h", SRC / "guard.cpp", SRC / "exception_policy.h", HERE / "object_identity_read.cpp", Path(__file__), HERE / "entry_hotpath.py"]
    report = {"pass": True, "baseline_commit": args.baseline, "scope": __doc__, "results": results,
              "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
              "windows_executed": False, "game_tested": False,
              "limits": "Local guard faults and LastError require the separate actual Windows x86 Runtime scope fixture; portable counts are not measured Win32 syscalls or FPS."}
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
