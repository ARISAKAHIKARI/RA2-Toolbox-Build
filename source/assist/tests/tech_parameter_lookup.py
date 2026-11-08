#!/usr/bin/env python3
"""Exercise source-extracted Full/Enhancement candidates and every live guard.

Native stack/register layout is a fixture model. Signatures and world guards
are actual extracted production functions over private synthetic mapped bytes;
no Windows process, debug register, game instruction or GPU is exercised.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def block(text, start):
    begin = text.index(start)
    opening = text.index("{", begin)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[begin:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--before-ref", default="cc3f0117c233284cec1963a11c26d637d55817fe")
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    src = here.parent / "source"
    bridge_path = "source/assist/source/tech_ui_breakpoint_bridge.h"
    paths = [bridge_path, "source/assist/source/tech_parameter_lookup.h",
             "source/assist/source/tech_enhancement_parameter_policy.h",
             "source/assist/source/tech_enhancement.h", "source/assist/source/technology_mode_policy.h",
             "source/assist/source/tech_ui_breakpoint_hot.h",
             "source/assist/tests/tech_parameter_lookup.cpp", "source/assist/tests/tech_parameter_lookup.py"]
    sha = lambda data: hashlib.sha256(data).hexdigest()
    hashes = {name: sha((repo / name).read_bytes()) for name in paths}
    current = (repo / bridge_path).read_text()
    previous = subprocess.check_output(["git", "show", args.before_ref + ":" + bridge_path], cwd=repo, text=True)
    old_flags = subprocess.check_output(["git", "show", args.before_ref + ":source/assist/source/tech_enhancement_parameter_policy.h"], cwd=repo, text=True)
    report = {"schema": 1, "before_commit": args.before_ref, "source_sha256": hashes,
              "scope": __doc__, "assertions_enabled": True, "game_tested": False,
              "windows_executed": False, "multiplayer_tested": False, "gpu_tested": False,
              "baseline_model": "Both phases use the current Policy storage fixture. The baseline uses the unmodified pinned callback/world/signature/fullType functions; it never reads the added index. Index lifecycle-only cases run in the current phase."}
    args.out.mkdir(parents=True, exist_ok=True)
    for phase, source in (("before", previous), ("after", current)):
        out = args.out / phase
        out.mkdir(exist_ok=True)
        pieces = [block(current, "struct " + name) + ";" for name in ("TypeIdentity", "EnhancedIdentity", "Policy")]
        pieces += ["static Policy policy;", block(source, "inline unsigned hash("),
                   block(source, "static bool world(").replace("world()", "worldImpl()", 1),
                   "static bool world(){++worldCalls;inWorld=true;const bool ok=worldImpl();inWorld=false;return ok;}",
                   block(source, "static bool nativeCompatibleHot(").replace("nativeCompatibleHot()", "nativeCompatibleHotImpl()", 1),
                   "static bool nativeCompatibleHot(){++nativeChecks;return nativeCompatibleHotImpl();}",
                   block(source, "static bool enhancementCompatibleHot(").replace("enhancementCompatibleHot()", "enhancementCompatibleHotImpl()", 1),
                   "static bool enhancementCompatibleHot(){++enhancementChecks;return enhancementCompatibleHotImpl();}"]
        if "static const TypeIdentity* fullIdentity(" in source:
            pieces += [block(source, "static const TypeIdentity* fullIdentity(")]
        pieces += [block(source, "static bool fullType("), block(source, "static bool captureWorld(")]
        full = block(source, "if(tag==0){").replace("reinterpret_cast<uint32_t*>(uintptr_t(context.Esp))", "fullStack")
        callback = block(source, 'extern "C" int __cdecl AssistTechUiParameter(')
        enhancement = callback[callback.index("if(!policy.enhanced"):].rsplit("}", 1)[0]
        pieces += ["static int fullDispatch(Context&context){const unsigned tag=0;" + full + "return 0;}",
                   "static int enhancementDispatch(Context&context,unsigned tag){" + enhancement + "}"]
        subject = "\n\n".join(pieces) + "\n"
        (out / "subject.inc").write_text(subject)
        oracle = block(old_flags, "inline uint32_t compareFlags(").replace("compareFlags(", "originalCompareFlags(", 1)
        (out / "flags-oracle.inc").write_text(oracle + "\n")
        exe = out / "tech-parameter-lookup"
        command = [args.cxx, "-std=c++17", "-O2", "-UNDEBUG", "-fno-exceptions", "-fno-rtti",
                   "-DCURRENT_PHASE=" + str(int(phase == "after")), "-I" + str(src), "-I" + str(out),
                   str(here / "tech_parameter_lookup.cpp"), "-o", str(exe)]
        built = subprocess.run(command, text=True, capture_output=True)
        (out / "compile.log").write_text(built.stdout + built.stderr)
        if built.returncode:
            print(built.stdout + built.stderr)
            raise SystemExit(built.returncode)
        run = subprocess.run([str(exe.resolve())], text=True, capture_output=True)
        (out / "run.log").write_text(run.stdout + run.stderr)
        if run.returncode:
            print(phase, run.stdout, run.stderr)
            raise SystemExit(run.returncode)
        report[phase] = json.loads(run.stdout)
        report[phase]["subject_sha256"] = sha(subject.encode())
        report[phase]["flags_oracle_sha256"] = sha(oracle.encode())
        print(phase, run.stdout.strip())
    report["source_unchanged"] = hashes == {name: sha((repo / name).read_bytes()) for name in paths}
    report["required_checks_passed"] = report["source_unchanged"] and report["before"]["pass"] and report["after"]["pass"]
    (args.out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    raise SystemExit(0 if report["required_checks_passed"] else 1)


if __name__ == "__main__":
    main()
