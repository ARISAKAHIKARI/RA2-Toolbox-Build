#!/usr/bin/env python3
"""Exercise production Full/control admission and retirement after local errors.

The Windows DR service is an explicit model. Production configure, maintain,
request, restore and group-retirement functions are extracted unchanged. No
game code, hardware registers or Windows APIs execute in this fixture.
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
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--before-ref", default="d108ab8")
    parser.add_argument("--after-only", action="store_true")
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    header = repo / "source/assist/source/tech_ui_breakpoint_bridge.h"
    core = repo / "source/assist/source/tech_ui_breakpoint_core.h"
    fixture = Path(__file__).with_suffix(".cpp")
    cases = ["full_transient_signature", "control_transient_signature",
             "full_service_failure", "control_service_failure", "both_service_failure",
             "full_foreign_failure", "control_thread_failure",
             "full_persistent_signature", "control_persistent_signature",
             "full_healthy_sibling", "control_healthy_sibling",
             "full_old_lease", "control_old_lease", "full_uncertain_lease",
             "control_uncertain_lease", "full_pending_transaction", "control_pending_transaction",
             "full_failed_retirement", "control_failed_retirement",
             "full_clock_wrap", "control_clock_wrap", "full_clock_zero", "control_clock_zero",
             "full_disable_reset", "control_disable_reset", "stop_no_reactivation",
             "full_world_recheck", "control_world_recheck", "full_restore_recheck"]
    before_cases = cases[:4]
    expected = {"full_transient_signature": "FAIL Full recovers after a transient signature failure",
                "control_transient_signature": "FAIL control recovers after a transient signature failure",
                "full_service_failure": "FAIL Full recovers after its service failure retires",
                "control_service_failure": "FAIL control recovers after its service failure retires"}
    hashes = {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in (header, core, fixture, Path(__file__))}
    report = {"schema": 1, "assertions_enabled": True, "game_tested": False,
              "windows_executed": False, "source_sha256": hashes,
              "before_commit": None if args.after_only else args.before_ref,
              "boundary": "Unmodified production admission, shared configure, maintenance, restoration, and group retirement functions; signature/world reads and the Windows DR worker are explicitly modeled. No native game instructions execute.",
              "before": [], "after": []}
    for phase in (("after",) if args.after_only else ("before", "after")):
        production = (header.read_text() if phase == "after" else subprocess.check_output(
            ["git", "show", args.before_ref + ":" + str(header.relative_to(repo))], cwd=repo).decode())
        globals_ = production[production.index("static int failure=0,"):production.index("static volatile LONG fullExceptions=")]
        pieces = ["namespace techparameters {", globals_]
        for marker in ("static bool healthy(", "static bool ready(", "static bool groupOff("):
            pieces.append(block(production, marker))
        if "static bool retryFailure(" in production:
            pieces.append(block(production, "static bool retryFailure("))
        pieces.extend([block(production, "static bool configure("), "}"])
        for marker in ("static bool techParametersRetired(", "static bool restoreTechParameters(",
                       "static void maintainTechParameters(", "static bool requestTechParameters(",
                       "static bool controlParametersRetired(", "static bool restoreControlParameters(",
                       "static int requestControlParameters("):
            pieces.append(block(production, marker))
        subject = "\n\n".join(pieces) + "\n"
        work = args.build_dir / phase
        work.mkdir(parents=True, exist_ok=True)
        (work / "tech_parameter_recovery_subject.inc").write_text(subject)
        report[phase + "_subject_sha256"] = hashlib.sha256(subject.encode()).hexdigest()
        exe = work / "tech-parameter-recovery"
        cmd = [args.compiler, "-std=c++17", "-O2", "-UNDEBUG", "-fno-exceptions", "-fno-rtti",
               "-I" + str(work), str(fixture), "-o", str(exe)]
        built = subprocess.run(cmd, capture_output=True, text=True)
        (work / "compile.log").write_text(built.stdout + built.stderr)
        if built.returncode:
            raise RuntimeError(built.stdout + built.stderr)
        for case in (before_cases if phase == "before" else cases):
            run = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=20)
            (work / (case + ".log")).write_text(run.stdout + run.stderr)
            row = {"case": case, "returncode": run.returncode,
                   "stdout": run.stdout.strip(), "stderr": run.stderr.strip()}
            if run.returncode == 0:
                row.update(json.loads(run.stdout))
            if phase == "before":
                row["expected_defect_reproduced"] = run.returncode == 1 and run.stderr.strip() == expected[case]
            report[phase].append(row)
            print(phase, case, "PASS" if run.returncode == 0 else "FAIL")
    report["checks"] = sum(row.get("checks", 0) for row in report["after"])
    report["cases"] = len(report["after"])
    report["before_reproduced"] = None if args.after_only else all(row["expected_defect_reproduced"] for row in report["before"])
    report["source_unchanged"] = hashes == {name: hashlib.sha256((repo / name).read_bytes()).hexdigest() for name in hashes}
    report["required_checks_passed"] = report["source_unchanged"] and (args.after_only or report["before_reproduced"]) and all(row["returncode"] == 0 for row in report["after"])
    (args.build_dir / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("cases", "checks", "before_reproduced", "required_checks_passed")}))
    raise SystemExit(0 if report["required_checks_passed"] else 1)


if __name__ == "__main__":
    main()
