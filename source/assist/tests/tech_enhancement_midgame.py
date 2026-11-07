#!/usr/bin/env python3
"""Compile actual activation/catalogue functions against explicit native models.

The before control extracts the same functions from a pinned Git commit. No
private executable, Windows execution, debug registers, game or netplay is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def function(source, signature):
    begin = source.index(signature)
    opening = source.index("{", begin)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build-dir", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--before-ref", default="1d7f3020b12c8f25b8b83e161ddd1bafc6364cda")
    p.add_argument("--before-catalogue-ref", default="58f847d3d2616c1c1dce8a55e0b4cddacc027d8e",
                   help="Frozen civilian catalogue admission control; separate from the original refresh control")
    p.add_argument("--skip-refresh-before", action="store_true",
                   help="Skip only the historic refresh baseline when its Git objects are unavailable; still run the civilian baseline")
    p.add_argument("--after-only", action="store_true", help="Verify the current source snapshot without a Git baseline comparison")
    p.add_argument("--compiler", default="g++")
    args = p.parse_args()
    repo = Path(__file__).resolve().parents[3]
    src = repo / "source/assist/source"
    fixture = Path(__file__).with_suffix(".cpp")
    paths = ["source/assist/source/bridge.cpp", "source/assist/source/protocol.h",
             "source/assist/source/sidebar.h", "source/assist/source/sidebar_policy.h",
             "source/assist/source/tech_enhancement.h", "source/assist/source/tech_enhancement_bridge.h",
             "source/assist/source/tech_ui_breakpoint_bridge.h", "source/assist/source/tech_enhancement_parameter_policy.h",
             "source/assist/source/technology_mode_policy.h", "source/assist/source/session_epoch.h",
             "source/external_reader/source/catalogue_stamp.h",
             "source/assist/tests/tech_enhancement_midgame.py", "source/assist/tests/tech_enhancement_midgame.cpp"]
    sha = lambda b: hashlib.sha256(b).hexdigest()
    hashes = {name: sha((repo / name).read_bytes()) for name in paths}
    cases = ["legacy_midgame_refresh", "legacy_recheck_once", "legacy_inactive_master", "legacy_active_master",
             "legacy_pending_master", "legacy_no_change", "legacy_unreadable_world", "exact_midgame_catalogue",
             "exact_pending_readiness", "exact_off_reenable", "exact_native_rule_rejected", "exact_factory_missing",
             "legacy_civilian_bus", "legacy_civilian_pres", "exact_civilian_bus", "exact_civilian_pres",
             "exact_civilian_inactive", "exact_civilian_not_whitelisted", "exact_civilian_wrong_vtable",
             "exact_civilian_wrong_array", "exact_civilian_unreadable", "exact_civilian_ineligible_level",
             "legacy_civilian_unowned", "legacy_civilian_conflict", "legacy_civilian_wrong_index",
             "legacy_civilian_world_changed", "legacy_civilian_world_unreadable", "legacy_civilian_stopped", "legacy_civilian_fault",
             "exact_civilian_missing_prerequisite", "exact_civilian_factory_owner_mismatch",
             "exact_filter_spawned", "exact_filter_no_owner", "exact_full_capacity",
             "exact_failed_signature", "exact_dependency_canbuild", "exact_dependency_factory", "exact_dependency_tab", "exact_dependency_addcameo",
             "exact_dependency_active_handoff", "production_dependency_canbuild", "exact_ares_body_bypassed", "rules_to_exact_ares_busy_handoff", "exact_full_allowlist", "exact_busy_mode_handoff"]
    report = {"schema": 2, "assertions_enabled": True, "game_tested": False, "multiplayer_tested": False,
              "windows_executed": False, "source_sha256": hashes, "before_commit": None if args.after_only or args.skip_refresh_before else args.before_ref,
              "refresh_before_skipped": bool(args.after_only or args.skip_refresh_before),
              "before_catalogue_commit": None if args.after_only else args.before_catalogue_ref,
              "boundary": "Actual production activation, restoration, effective catalogue filtering, type selection, and cameo gate functions; native eligibility/factory/AddCameo, signatures, and DR readiness are modeled; memory is private synthetic mmap data. No native game instructions execute.",
              "civilian_rules": "PRES/BUS Insignificant=yes and their different owner/prerequisite relationships are modeled from agreeing public original-rule candidates. Owner groups and the prerequisite index are synthetic; no user MOD rules or live sidebar were read. The original refresh controls retain their generic Insignificant=false BUS fixture.",
              "before": [], "before_catalogue": [], "after": []}
    expected_before = {
        "legacy_midgame_refresh": "FAIL legacy activation requests native House tech-tree evaluation",
        "exact_midgame_catalogue": "FAIL exact activation invalidates the formerly filtered catalogue",
    }
    expected_catalogue_before = {
        "legacy_civilian_bus": "FAIL legacy enhanced BUS with Insignificant enters the catalogue",
        "legacy_civilian_pres": "FAIL legacy enhanced PRES with Insignificant enters the catalogue",
        "exact_civilian_bus": "FAIL exact enhanced BUS with Insignificant enters the catalogue",
        "exact_civilian_pres": "FAIL exact enhanced PRES with Insignificant enters the catalogue",
    }
    args.build_dir.mkdir(parents=True, exist_ok=True)
    phases = ("after",) if args.after_only else ("before_catalogue", "after") if args.skip_refresh_before else ("before", "before_catalogue", "after")
    for phase in phases:
        work = args.build_dir / phase
        work.mkdir(exist_ok=True)
        def source(name):
            rel = "source/assist/source/" + name
            reference = args.before_ref if phase == "before" else args.before_catalogue_ref
            return (subprocess.check_output(["git", "show", reference + ":" + rel], cwd=repo).decode()
                    if phase != "after" else (src / name).read_text())
        bridge, sidebar, legacy, exact = (source(n) for n in ("bridge.cpp", "sidebar.h", "tech_enhancement_bridge.h", "tech_ui_breakpoint_bridge.h"))
        pieces = [legacy[:legacy.index("static bool enhancementWrite(")],
                  function(source("protocol.h"), "inline bool validType("),
                  "namespace techparameters {" + function(exact, "static bool enhancedRequested(") + "}",
                  function(exact, "static int techEnhancementCatalogueLevel(")]
        if "static bool techEnhancementCatalogueIncludesInsignificant(" in exact:
            pieces += ["static techenhancement::IdentityState enhancementWorld();",
                       function(exact, "static bool techEnhancementCatalogueIncludesInsignificant(")]
        pieces.append(function(bridge, "static void scanTypes("))
        for sig in ("static int typeSlot(uint32_t type)", "static int canonicalKind(", "static int typeTab(",
                    "static void sidebarRedraw(", "static bool hasCameo(", "static uint32_t techFactory(",
                    "static bool techCameoInventory(", "static void requestTechRefresh("):
            pieces.append(function(sidebar, sig))
        for sig in ("static techenhancement::IdentityState enhancementWorld(", "static bool techLegacyEnhancementOwns(",
                    "static bool techLegacyEnhancementProductionPending(", "static bool restoreTechEnhancement(",
                    "static void techEnhancementTick("):
            pieces.append(function(legacy, sig))
        for sig in ("static bool techEnhancementProductionPending(", "static void deferTechEnhancementParameters(",
                    "static void techEnhanceParametersTick("):
            pieces.append(function(exact, sig))
        subject = "\n\n".join(pieces)
        (work / "tech_midgame_subject.inc").write_text(subject)
        exe = work / "tech-midgame"
        command = [args.compiler, "-std=c++17", "-O2", "-UNDEBUG", "-fno-exceptions", "-fno-rtti",
                   "-I" + str(src), "-I" + str(repo / "source/external_reader/source"), "-I" + str(work), str(fixture), "-o", str(exe)]
        built = subprocess.run(command, text=True, capture_output=True)
        (work / "compile.log").write_text(built.stdout + built.stderr)
        if built.returncode:
            print(built.stdout + built.stderr)
            raise SystemExit(built.returncode)
        report[phase + "_subject_sha256"] = sha(subject.encode())
        phase_cases = list(expected_before) if phase == "before" else list(expected_catalogue_before) if phase == "before_catalogue" else cases
        for case in phase_cases:
            run = subprocess.run([str(exe), case], text=True, capture_output=True)
            (work / (case + ".log")).write_text(run.stdout + run.stderr)
            row = {"case": case, "status": "PASS" if not run.returncode else "FAIL", "returncode": run.returncode,
                   "stdout": run.stdout.strip(), "stderr": run.stderr.strip()}
            if not run.returncode:
                row.update(json.loads(run.stdout))
            if phase != "after":
                expected = expected_before if phase == "before" else expected_catalogue_before
                row["expected_defect_reproduced"] = run.returncode == 1 and run.stderr.strip() == expected[case]
            report[phase].append(row)
            print(phase, case, row["status"])
    report["checks"] = sum(row.get("checks", 0) for row in report["after"])
    report["cases"] = len(report["after"])
    report["source_unchanged"] = hashes == {name: sha((repo / name).read_bytes()) for name in paths}
    report["before_reproduced"] = None if args.after_only or args.skip_refresh_before else all(row["expected_defect_reproduced"] for row in report["before"])
    report["before_catalogue_reproduced"] = None if args.after_only else all(row["expected_defect_reproduced"] for row in report["before_catalogue"])
    report["required_checks_passed"] = report["source_unchanged"] and (args.after_only or ((args.skip_refresh_before or report["before_reproduced"]) and report["before_catalogue_reproduced"])) and all(row["status"] == "PASS" for row in report["after"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("cases", "checks", "before_reproduced", "before_catalogue_reproduced", "required_checks_passed")}))
    raise SystemExit(0 if report["required_checks_passed"] else 1)


if __name__ == "__main__":
    main()
