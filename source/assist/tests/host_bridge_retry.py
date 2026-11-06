"""Execute actual host connect/retry routes, with an optional old-source negative control."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def function(text, name):
    start = text.index(name)
    at = text.index("{", start)
    depth = 1
    end = at + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baseline-main", type=Path)
    parser.add_argument("--source-main", type=Path)
    parser.add_argument("--cxx", default="c++")
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    source = args.source_main or here.parent / "source/main.cpp"
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    variants = [("current", source)]
    if args.baseline_main:
        variants.insert(0, ("baseline", args.baseline_main))
    results = {}
    for name, path in variants:
        before = digest(path)
        text = path.read_text(encoding="utf-8-sig")
        names = ["static bool bridgeFailed("]
        if "static bool bridgeConnectFailed(" in text:
            names.append("static bool bridgeConnectFailed(")
        names.append("static bool ensureBridge(){")
        routes = output / (name + "-production.inc")
        routes.write_text("\n".join(function(text, n) for n in names) + "\n", encoding="utf-8")
        executable = output / name
        command = [args.cxx, "-std=c++17", "-O2", "-D_WIN64",
                   '-DHOST_BRIDGE_ROUTES="' + routes.as_posix() + '"',
                   str(here / "host_bridge_retry.cpp"), "-o", str(executable)]
        build = subprocess.run(command, text=True, capture_output=True)
        (output / (name + "-build.log")).write_text(build.stdout + build.stderr, encoding="utf-8")
        if build.returncode:
            raise SystemExit(build.stdout + build.stderr)
        run = subprocess.run([str(executable)], text=True, capture_output=True)
        (output / (name + ".log")).write_text(run.stdout + run.stderr, encoding="utf-8")
        if run.returncode not in (0, 1):
            raise SystemExit(run.stdout + run.stderr)
        report = json.loads(run.stdout.strip().splitlines()[-1])
        if name == "current":
            assert run.returncode == 0 and report["failures"] == 0, run.stdout + run.stderr
        else:
            assert run.returncode == 1 and report["failures"] > 0, "Old source did not reproduce the defect"
            for expected in ("blocked bridge must not enter runtimeAvailable",
                             "future retry must not enter runtimeAvailable",
                             "failure backoff starts after all blocking work"):
                assert "FAIL: " + expected in run.stdout, "Missing old-source counterexample: " + expected
        assert digest(path) == before, "Production source changed during the test"
        results[name] = {"source": str(path.resolve()), "source_sha256": before,
                         "routes_sha256": digest(routes), "exit_code": run.returncode,
                         "build_command": command, **report}
    report = {"schema": "ra2.host-bridge-retry.v1", "passed": True,
              "real_windows_tested": False, "game_tested": False,
              "results": results}
    (output / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
