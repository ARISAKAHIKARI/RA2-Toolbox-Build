#!/usr/bin/env python3
"""Run real Full admission/callback functions against Runtime10's entry bytes.

Native chain admission is an explicit model here; native_entry_callability
tests cover that independent boundary. No game instructions, DR or VEH execute.
"""
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
    src = Path(__file__).resolve().parents[1] / "source"
    header = src / "tech_ui_breakpoint_bridge.h"
    fixture = Path(__file__).with_suffix(".cpp")
    production = header.read_text()
    declarations = production[production.index("static techbp::SignatureSnapshot<20> nativeDraw;"):production.index("static uint32_t nativeEntryDigest(")]
    subject = "namespace techparameters {\n" + declarations + "\n".join(block(production, marker) for marker in (
        "static uint32_t nativeEntryDigest(", "static bool captureNativeEntry(",
        "static bool nativeCompatible(", "static bool enhancementBodyBypassed(", "static bool nativeCompatibleHot(")) + "\n}\n"
    callback = block(production, 'extern "C" int __cdecl AssistTechUiParameter(')
    subject += "static int applyFull(CONTEXT& context){using namespace techparameters;const unsigned tag=0;\n" + block(callback, "if(tag==0){") + "\nreturn 0;}\n"
    args.build_dir.mkdir(parents=True, exist_ok=True)
    (args.build_dir / "tech_ares_entry_subject.inc").write_text(subject)
    executable = args.build_dir / "tech-ares-entry-compat"
    command = [args.compiler, "-std=c++17", "-O2", "-UNDEBUG", "-fno-exceptions", "-fno-rtti", "-I" + str(src), "-I" + str(args.build_dir), str(fixture), "-o", str(executable)]
    built = subprocess.run(command, capture_output=True, text=True)
    (args.build_dir / "compile.log").write_text(built.stdout + built.stderr)
    if built.returncode:
        raise RuntimeError(built.stdout + built.stderr)
    ran = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
    (args.build_dir / "run.log").write_text(ran.stdout + ran.stderr)
    if ran.returncode:
        raise RuntimeError(ran.stdout + ran.stderr)
    result = json.loads(ran.stdout)
    result["subject_sha256"] = hashlib.sha256(subject.encode()).hexdigest()
    shared = src.parents[1] / "shared/source"
    paths = [header, src / "tech_ui_breakpoint_hot.h", src / "tech_ui_breakpoint_debug.h", src / "map_visibility_parameter_policy.h", fixture, Path(__file__)]
    paths += [shared / name for name in ("debug_entry_types.h", "debug_entry_policy.h", "debug_entry_catalog.h")]
    result["source_sha256"] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    (args.build_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(ran.stdout, end="")


if __name__ == "__main__":
    main()
