"""Exercise production tactical collectors over a real sealed raw Snapshot.

The source functions and SampleReadScope are extracted without changing their
bodies. Synthetic process memory and native/event seams are explicit. These are
CPU regressions, not Windows gameplay, frame-rate or multiplayer measurements.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "source"
SHARED = SRC.parents[1] / "shared" / "source"


def function(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def production():
    names = ["cooperative_bridge.h", "external_orders_bridge.h", "dodge_bridge.h",
             "reverse_dodge_bridge.h", "missile_bridge.h", "autoload_bridge.h",
             "unload_compute_bridge.h", "native_orders_bridge.h", "world_frame_runtime.h", "bridge.cpp",
             "harvester_defense_bridge.h", "harvester_defense_raw.h", "compute_tasks.h",
             "compute_input_copy.h", "missile_compute.h", "reverse_target_index.h"]
    source = {name: (SRC / name).read_text() for name in names}
    parts = []
    add = parts.append
    orders = source["external_orders_bridge.h"]
    for signature in ("static bool oreTypeAllowed(", "static bool oreMiner(",
                      "static bool rawOreCompatible(", "static bool rawDockBusy(", "static bool rawDockLinked("):
        add(function(orders, signature))
    add('#include "raw_combat.h"')
    add(function(source["harvester_defense_raw.h"], "static bool minerDefenseMiner("))
    add(function(source["harvester_defense_raw.h"], "static bool minerDefenseDock("))
    dodge = source["dodge_bridge.h"]
    add(dodge[:dodge.index("static void finishDodge(")])
    for signature in ("static bool dodgeSource(", "static int dodgeThreat(", "static bool dodgeBusy(",
                      "static bool dodgeSeen(", "static void dodgeObserve(", "static bool collectDodgeInput("):
        add(function(dodge, signature))
    add('#include "combat_query_bridge.h"\n#include "combat_probe_bridge.h"')
    coop = source["cooperative_bridge.h"]
    add(function(coop, "struct CooperativeFrameRead") + ";")
    add(function(coop, "static bool collectCooperativeInput("))
    load = source["autoload_bridge.h"]
    add(load[load.index("static bool loadCalculationPending,"):load.index("static int submitLoading(")])
    for signature in ("static bool loadingObject(", "static uint32_t incomingVehicle(",
                      "static loading::Observation loadingObservation(", "static int passengerSlots("):
        add(function(load, signature))
    add("static bool loadQueryFailed;")
    add(function(load, "static bool queryEnterAction("))
    add(function(load, "static bool canEnter("))
    for signature in ("static bool captureLoadSelection(", "static bool collectLoadFacts(",
                      "static int autoLoad(uint32_t house,const compute::LoadInput*cohort){"):
        add(function(load, signature))
    unload = source["unload_compute_bridge.h"]
    add(function(source["native_orders_bridge.h"], "struct UnloadPassenger") + ";")
    for signature in ("static unloadcompute::View unloadView(", "static bool unloadSourceCurrent(",
                      "static uint8_t unloadTargetAction("):
        add(function(unload, signature))
    add(unload[unload.index("static unloadcompute::Target unloadSampleTargets"):unload.index("static bool applyUnloadDecision(")])
    missile = source["missile_bridge.h"]
    add(missile[:missile.index("static void pruneMissileSends(")])
    add(function(missile, "static bool collectMissileInput("))
    add('#include "planner_world_copy.h"\n#include "anti_theft_sample_bridge.h"')
    add("static planner::World planningWorld,planningCapture;static int planningFrame=-1;static uint32_t planningSerial,inputSerial;")
    add(function(orders, "static bool publishPlanningWorld("))
    add(function(source["bridge.cpp"], "static int collectRepairs("))
    add('#include "ore_harvest_bridge.h"')
    add("static std::unordered_map<uint32_t,uint32_t> minerDefenseLinkedRefinery;static uint32_t minerDefenseRefineries[planner::MaxRefineries];static int minerDefenseRefineryCount;static bool minerDefenseRefineryIncomplete;")
    add(function(source["harvester_defense_bridge.h"], "static bool minerDefenseIndexRefineries("))
    reverse = source["reverse_dodge_bridge.h"]
    add('#include "reverse_dodge.h"\n#include "reverse_target_index.h"')
    add(function(reverse, "static bool reverseVector("))
    add(reverse[reverse.index("struct ReverseTarget"):reverse.index("static bool reverseDodgeTick(")])
    scope = source["world_frame_runtime.h"]
    scope_text = "static worldframe::DemandLeases runtimeDemandLeases;\n" + "\n".join(function(scope, signature) for signature in (
        "static void requestWorldRange(", "static bool readWorldSample(", "class SampleReadScope")) + ";\n"
    return "\n\n".join(parts) + "\n", scope_text, names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    content, scope, names = production()
    out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="tactical-frame-"))
    out.mkdir(parents=True, exist_ok=True)
    (out / "tactical_frame_production.h").write_text(content)
    (out / "sample_scope_production.h").write_text(scope)
    exe = out / "tactical-frame-sampling"
    flags = ["-std=c++17", "-O2", "-UNDEBUG", "-I", str(out), "-I", str(SRC)]
    build = subprocess.run([args.cxx, *flags, str(HERE / "tactical_frame_sampling.cpp"), "-o", str(exe)], capture_output=True, text=True)
    (out / "build.log").write_text(build.stdout + build.stderr)
    print(build.stdout + build.stderr, end="")
    build.check_returncode()
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    (out / "run.log").write_text(run.stdout + run.stderr)
    print(run.stdout + run.stderr, end="")
    run.check_returncode()
    paths = [SRC / n for n in names] + [SRC / "planner_world_copy.h", SRC / "anti_theft_sample_bridge.h", SRC / "tactical_frame_read.h", SRC / "raw_combat.h",
            SRC / "combat_probe_bridge.h", SRC / "ore_harvest_bridge.h", SHARED / "world_frame.h",
            HERE / "tactical_frame_sampling.cpp", Path(__file__).resolve(),
            HERE / "fixtures/tactical_frame/autoload_before.h"]
    report = {"pass": True, "scope": __doc__, "flags": flags, "output": run.stdout,
              "windows_executed": False, "game_process_tested": False, "multiplayer_tested": False,
              "extracted_sha256": {"tactical_frame_production.h": hashlib.sha256(content.encode()).hexdigest(),
                                   "sample_scope_production.h": hashlib.sha256(scope.encode()).hexdigest()},
              "source_sha256": {str(p.relative_to(SRC.parents[1])): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
              "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest()}
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
