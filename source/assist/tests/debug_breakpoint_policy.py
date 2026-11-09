#!/usr/bin/env python3
"""Run production DR reference policy and diagnostic state on synthetic memory."""
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
    parser.add_argument("--baseline-bridge", type=Path, help="Fixed previous production bridge for admission equivalence and read-count comparison")
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    source = tests.parent / "source"
    bridge = (source / "tech_ui_breakpoint_bridge.h").read_text()
    core = (source / "tech_ui_breakpoint_core.h").read_text()
    fps = (source / "single_player_fps_runtime.h").read_text()
    args.build_dir.mkdir(parents=True, exist_ok=True)
    lease = "namespace assist { namespace techbp {\n" + core[core.index("inline constexpr unsigned SiteCount="):core.index("// These two instructions")] + "}}\n"
    (args.build_dir / "debug_breakpoint_lease.inc").write_text(lease)
    (args.build_dir / "debug_breakpoint_active.inc").write_text(block(core, "uint32_t activeTags()"))
    (args.build_dir / "debug_breakpoint_fps.inc").write_text(fps[fps.index("namespace assist { namespace singlefps {"):])
    globals_ = bridge[bridge.index("static bool mapDesired="):bridge.index("inline unsigned hash(")]
    declarations = bridge[bridge.index("static techbp::SignatureSnapshot<20> nativeDraw;"):bridge.index("static uint32_t nativeEntryDigest(")]
    functions = ["static uint32_t nativeEntryDigest(", "static bool captureNativeEntry(", "static bool nativeCompatible(",
                 "static bool enhancementCompatible(", "static bool enhancementBodyBypassed(", "static bool nativeCompatibleHot(",
                 "static bool enhancementCompatibleHot(", "static bool mapCompatible(", "static bool controlCompatible(",
                 "static bool enhancedRequested(", "static bool healthy(", "static bool ready(", "static bool groupOff(",
                 "static void syncDebugBreakpointPolicy(", "static void inspectDebugBreakpointSignatures(", "static void publishDebugBreakpointStates("]
    subject = "namespace techparameters {\n" + globals_ + declarations + "static bool debugExactUnreachable=false;\n" + "\n".join(block(bridge, f) for f in functions) + "\n}\n" + block(bridge, "static bool mapVisibilityParameter(")
    baseline_text = args.baseline_bridge.read_text() if args.baseline_bridge else None
    if baseline_text is not None:
        baseline_admission = block(baseline_text, "static bool controlCompatible(").replace("static bool controlCompatible(", "static bool controlCompatibleBaseline(", 1)
        subject += "\nnamespace techparameters {\n" + baseline_admission + "\n}\n"
    (args.build_dir / "debug_breakpoint_subject.inc").write_text(subject)
    exe = args.build_dir / "debug-breakpoint-policy"
    command = [args.compiler, "-std=c++17", "-O2", "-UNDEBUG", "-fno-exceptions", "-fno-rtti", "-I" + str(source), "-I" + str(args.build_dir), str(tests / "debug_breakpoint_policy.cpp"), "-o", str(exe)]
    if baseline_text is not None:
        command.insert(1, "-DCONTROL_ADMISSION_BASELINE")
    build = subprocess.run(command, capture_output=True, text=True)
    (args.build_dir / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    (args.build_dir / "run.log").write_text(run.stdout + run.stderr)
    if run.returncode:
        raise RuntimeError(run.stdout + run.stderr)
    result = json.loads(run.stdout)
    result["subject_sha256"] = hashlib.sha256((subject + lease + fps).encode()).hexdigest()
    names = ["tech_ui_breakpoint_bridge.h", "tech_ui_breakpoint_core.h", "tech_ui_breakpoint_hot.h", "tech_ui_breakpoint_debug.h", "single_player_fps_runtime.h", "single_player_fps.h", "read_only_lifecycle.h", "control_parameter_policy.h", "map_visibility_parameter_policy.h", "tech_enhancement_parameter_policy.h"]
    paths = [source / name for name in names] + [tests / "debug_breakpoint_policy.cpp", Path(__file__)]
    paths += [source.parents[1] / "shared/source" / name for name in ("debug_entry_types.h", "debug_entry_policy.h", "debug_entry_catalog.h", "master_control_policy.h", "patch_context.h")]
    result["source_sha256"] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    if baseline_text is not None:
        result["baseline_bridge_sha256"] = hashlib.sha256(args.baseline_bridge.read_bytes()).hexdigest()
    (args.build_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result | {"source_sha256": "recorded"}))


if __name__ == "__main__":
    main()
