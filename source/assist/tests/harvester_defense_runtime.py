"""Exercise production miner-defense/ore/raw-combat code on synthetic memory.

The test reads no game process and invokes no native game code. The explicit
boundaries are private raw-memory pages, snapshot publication, feature ownership,
and nativeMission (capturing the production intent::mega event representation).
Defense decisions, raw weapon/threat classification, ore eligibility, identities,
map search, manual revisions and task arbiters are the real production functions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "source"


def function(text, needle):
    start = text.index(needle)
    brace = text.index("{", start)
    at, depth = brace + 1, 1
    while depth:
        depth += (text[at] == "{") - (text[at] == "}")
        at += 1
    return text[start:at]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--group", help="Run only cases whose name contains this text")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    external = (SRC / "external_orders_bridge.h").read_text(encoding="utf-8")
    native = (SRC / "native_orders_bridge.h").read_text(encoding="utf-8")
    extract = "\n".join(function(external, needle) for needle in (
        "static bool refreshOreConfig(", "static bool oreTypeAllowed(",
        "static bool oreMiner(", "static uint32_t worldByID(",
        "static uint32_t rawObjectID(", "static uint32_t readMapCell(",
        "static bool rawOreCompatible("))
    extract += "\n" + function(native, "static bool nativeEnemy(") + "\n"
    (args.out / "miner_runtime_extract.h").write_text(extract, encoding="utf-8")
    manual_extract = function(external, "static void cancelOreJob(") + "\n"
    (args.out / "miner_manual_extract.h").write_text(manual_extract, encoding="utf-8")
    names = ["harvester_defense.h", "harvester_defense_raw.h", "harvester_defense_bridge.h",
             "ore_harvest_bridge.h", "ore_harvest_search.h", "raw_combat.h",
             "external_orders_bridge.h", "native_orders_bridge.h", "bridge.cpp", "protocol.h",
             "planner_types.h", "planner_index.h", "weapon_policy.h", "object_index.h",
             "unit_task_arbiter.h", "command_intent.h", "ore_type_config.h", "ore_queue.h",
             "feature_type_config.h", "feature_type_bridge.h"]
    paths = [SRC / name for name in names] + [HERE / "harvester_defense_runtime.cpp", Path(__file__)]
    hashes = {str(path.relative_to(HERE.parents[1])): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in paths}
    exe = args.out / "harvester_defense_runtime"
    cmd = [args.cxx, "-std=c++17", "-O2", "-g", "-UNDEBUG", "-I" + str(args.out.resolve()),
           str(HERE / "harvester_defense_runtime.cpp"), "-o", str(exe)]
    build = subprocess.run(cmd, capture_output=True, text=True)
    (args.out / "build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
    print(build.stdout + build.stderr, end="")
    build.check_returncode()
    run_cmd = [str(exe)] + ([args.group] if args.group else [])
    run = subprocess.run(run_cmd, capture_output=True, text=True, timeout=30)
    (args.out / "runtime.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    print(run.stdout + run.stderr, end="")
    report = {"scope": __doc__, "host_execution": "Linux g++ synthetic-memory fixture",
              "game_process_operated": False, "windows_or_multiplayer_claim": False,
              "source_sha256": hashes, "production_extract_sha256": hashlib.sha256(extract.encode()).hexdigest(),
              "production_manual_extract_sha256": hashlib.sha256(manual_extract.encode()).hexdigest(),
              "exit_code": run.returncode,
              "sources_stable_during_run": all(hashlib.sha256(path.read_bytes()).hexdigest() == hashes[str(path.relative_to(HERE.parents[1]))] for path in paths)}
    try:
        report["result"] = json.loads(run.stdout)
    except json.JSONDecodeError:
        report["output"] = run.stdout + run.stderr
    (args.out / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    run.check_returncode()


if __name__ == "__main__":
    main()
