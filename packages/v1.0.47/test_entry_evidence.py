#!/usr/bin/env python3
"""Acceptance/rejection tests for entry receipt policy, using synthetic receipts.

This suite does not execute production, create release evidence, or claim game
acceptance. Fixed historical headers are real bytes solely to test identity
pinning; all execution streams are explicitly generated policy fixtures.
"""
from pathlib import Path
from types import SimpleNamespace
import argparse
import copy
import hashlib
import importlib.util
import json
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, HERE / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ENTRY = load("entry_evidence")
FIXTURES = load("entry_policy_fixtures")
PACKAGE = SimpleNamespace(ENTRY=ENTRY, sha=ENTRY.digest)


class EntryEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="entry-policy-only-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        paths = set().union(*(set().union(*bindings) for bindings in ENTRY.BINDINGS.values()))
        self.sources = {p: ("Synthetic policy source: " + p).encode() for p in paths}
        for path in paths:
            if "/fixtures/" in path:
                self.sources[path] = (ROOT / path).read_bytes()
        self.inputs = {p: {"sha256": ENTRY.digest(data)} for p, data in self.sources.items()}
        self.fixture("read_only_lifecycle_integration")

    def fixture(self, name):
        self.name = name
        text, supporting = FIXTURES.make(PACKAGE, name, self.sources)
        self.report = ENTRY.unique(text)
        self.path = self.directory / name / "result.json"
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(text)
        self.check = {"supporting_logs": {}}
        for field, paths in zip(("test_source_hashes", "production_source_hashes"), ENTRY.BINDINGS[name]):
            self.check[field] = {p: self.inputs[p]["sha256"] for p in paths}
        for relative, raw in supporting.items():
            path = self.path.parent / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            self.check["supporting_logs"][str(path)] = {"bytes": len(raw), "sha256": ENTRY.digest(raw)}

    def verify(self, report=None, check=None):
        return ENTRY.validate(self.name, report or self.report, self.path, check or self.check,
                              self.inputs, self.sources.__getitem__)

    def malformed(self, change):
        report = copy.deepcopy(self.report)
        change(report)
        self.assertFalse(ENTRY.success(self.name, json.dumps(report)))

    def rebind(self, path):
        data = path.read_bytes()
        self.check["supporting_logs"][str(path)] = {"bytes": len(data), "sha256": ENTRY.digest(data)}

    def test_complete_synthetic_receipt_passes(self):
        logs = self.verify()
        self.assertEqual(len(logs), len(self.check["supporting_logs"]))
        self.assertEqual(self.report["checks"], 284)
        self.assertEqual(self.report["scenarios"], 69)

    def test_helper_minimum_and_typed_raw_summary(self):
        self.fixture("read_only_lifecycle")
        self.verify()
        for field, value in (("checks", 67), ("scenarios", 7), ("failures", False),
                             ("passed", 1), ("scope", "")):
            with self.subTest(field=field):
                self.malformed(lambda r: r.update({field: value}))
        self.assertFalse(ENTRY.success(self.name, '{"passed":true}'))

    def test_helper_source_binding_required(self):
        self.fixture("read_only_lifecycle")
        for field in ("test_source_hashes", "production_source_hashes"):
            check = copy.deepcopy(self.check)
            check[field].pop(next(iter(check[field])))
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.verify(check=check)

    def test_missing_or_passed_only_stage_rejected(self):
        for name in ENTRY.STAGES:
            with self.subTest(stage=name, mutation="missing"):
                self.malformed(lambda r: r["additional_stages"].pop(name))
            with self.subTest(stage=name, mutation="wrapper"):
                self.malformed(lambda r: r["additional_stages"][name].update(report={"passed": True}))

    def test_main_and_stage_counts_are_exact(self):
        for key in ("checks", "scenarios"):
            with self.subTest(total=key):
                self.malformed(lambda r: r.update({key: r[key] + 1}))
        for name in ("host_current", "runtime_current", "negative_control"):
            for key in ("checks", "scenarios", "failures"):
                with self.subTest(branch=name, key=key):
                    self.malformed(lambda r: r[name]["summary"].update({key: r[name]["summary"][key] + 1}))
            for key, value in (("passed", int(name != "negative_control")), ("failures", False)):
                with self.subTest(branch=name, untyped=key):
                    self.malformed(lambda r: r[name]["summary"].update({key: value}))
        for name in ENTRY.STAGES:
            with self.subTest(stage=name):
                self.malformed(lambda r: r["additional_stages"][name]["report"]["current"]["execution"].update(checks=1))

    def test_main_negative_is_compiled_assertion_failure(self):
        for field, value in (("compile_exit_code", 1), ("run_exit_code", 0),
                             ("run_exit_code", True), ("fixture_sha256", "0" * 64),
                             ("source_commit", "a" * 40), ("expected_failure_cases", [])):
            with self.subTest(field=field, value=value):
                self.malformed(lambda r: r["negative_control"].update({field: value}))

    def test_every_stage_requires_current_and_compiled_negative(self):
        for name in ENTRY.STAGES:
            for branch in ("current", "negative"):
                for field in ("compile_exit", "run_exit"):
                    def change(r):
                        child = r["additional_stages"][name]["report"]
                        row = child["current"] if branch == "current" else child["negative_control"]["result"]
                        row[field] = 1 if field == "compile_exit" or branch == "current" else 0
                    with self.subTest(stage=name, branch=branch, field=field):
                        self.malformed(change)

    def test_fixed_fixtures_and_negative_labels_are_pinned(self):
        for name in ENTRY.STAGES:
            for field, value in (("fixture_sha256", "0" * 64), ("expected_failures", []),
                                 ("observed_failures", ["FAIL unrelated assertion"]), ("passed", 1)):
                with self.subTest(stage=name, field=field):
                    self.malformed(lambda r: r["additional_stages"][name]["report"]["negative_control"].update({field: value}))

    def test_negative_sources_may_replace_only_fixed_subject(self):
        for name, spec in ENTRY.STAGES.items():
            def change(r):
                child = r["additional_stages"][name]["report"]
                path = next(p for p in child["current"]["sources"] if p != spec["replaced"])
                child["negative_control"]["result"]["sources"][path] = "a" * 64
            with self.subTest(stage=name):
                self.malformed(change)

    def test_stage_execution_and_driver_receipt_required(self):
        for name in ENTRY.STAGES:
            for field, value in (("exit_code", 1), ("command", []), ("report_sha256", ""),
                                 ("driver_log_sha256", ""), ("raw_logs", {})):
                with self.subTest(stage=name, field=field):
                    self.malformed(lambda r: r["additional_stages"][name].update({field: value}))

    def test_every_required_raw_binding_is_mandatory(self):
        paths = ["run.log", "host-current/run.log", "host-pre5-negative/run.log", "runtime-current/run.log"]
        for name in ENTRY.STAGES:
            paths += [name + "/" + suffix for suffix in ("driver.log", "result.json", "current/run.log", "negative-control/run.log")]
        for relative in paths:
            check = copy.deepcopy(self.check)
            check["supporting_logs"].pop(str(self.path.parent / relative))
            with self.subTest(path=relative), self.assertRaises(ValueError):
                self.verify(check=check)

    def test_missing_binary_file_rejected(self):
        path = self.path.parent / "input_pairs/current/run.log"
        path.unlink()
        with self.assertRaises(FileNotFoundError):
            self.verify()

    def test_changed_binary_log_rejected_even_after_external_rebind(self):
        path = self.path.parent / "fps_recovery/negative-control/run.log"
        path.write_bytes(path.read_bytes() + b"FAIL replaced after execution\n")
        self.rebind(path)
        with self.assertRaises(ValueError):
            self.verify()

    def test_report_file_must_match_nested_report_and_hash(self):
        path = self.path.parent / "cooperative_recovery/result.json"
        path.write_text('{"passed":true}\n')
        self.rebind(path)
        report = copy.deepcopy(self.report)
        report["additional_stages"]["cooperative_recovery"]["report_sha256"] = ENTRY.digest(path.read_bytes())
        with self.assertRaises(ValueError):
            self.verify(report)

    def test_rebound_raw_summary_cannot_disagree_with_execution(self):
        name = "fps_recovery"
        report = copy.deepcopy(self.report)
        stage = report["additional_stages"][name]
        path = self.path.parent / name / "current/run.log"
        bad = json.dumps({"scenarios": 6, "checks": 999, "failures": 0}).encode() + b"\n"
        path.write_bytes(bad)
        stage["raw_logs"]["current/run.log"] = ENTRY.digest(bad)
        stage["report"]["current"]["raw_run_sha256"] = ENTRY.digest(bad)
        child_path = self.path.parent / name / "result.json"
        child_path.write_text(json.dumps(stage["report"]) + "\n")
        stage["report_sha256"] = ENTRY.digest(child_path.read_bytes())
        self.rebind(path)
        self.rebind(child_path)
        with self.assertRaises(ValueError):
            self.verify(report)

    def test_current_source_cannot_be_relabelled_after_change(self):
        for path in ("source/assist/source/main.cpp", "source/assist/source/single_player_fps_runtime.h",
                     "source/assist/source/cooperative_bridge.h", "source/assist/source/native_message_policy.h"):
            original = self.sources[path]
            self.sources[path] += b"changed after run"
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.verify()
            self.sources[path] = original

    def test_stale_compiled_header_hash_rejected(self):
        report = copy.deepcopy(self.report)
        headers = report["runtime_current"]["header_dependencies"]
        headers[next(iter(headers))] = "a" * 64
        with self.assertRaises(ValueError):
            self.verify(report)

    def test_bound_extra_logs_are_allowed_but_must_match(self):
        path = self.path.parent / "extra-compile.log"
        path.write_bytes(b"Synthetic additional compiler observation\n")
        self.rebind(path)
        self.verify()
        self.check["supporting_logs"][str(path)]["bytes"] += 1
        with self.assertRaises(ValueError):
            self.verify()

    def test_supporting_files_cannot_escape_report_directory(self):
        outside = self.directory / "outside.log"
        outside.write_bytes(b"out of scope")
        self.rebind(outside)
        with self.assertRaises(ValueError):
            self.verify()

    def test_nested_raw_paths_cannot_escape_the_stage(self):
        report = copy.deepcopy(self.report)
        report["additional_stages"]["input_pairs"]["raw_logs"]["../../outside.log"] = "a" * 64
        with self.assertRaises(ValueError):
            self.verify(report)

    def test_duplicate_json_keys_and_wrapped_passes_are_rejected(self):
        text = json.dumps(self.report)
        duplicate = text.replace('"passed": true', '"passed": true, "passed": true', 1)
        self.assertFalse(ENTRY.success(self.name, duplicate))
        self.assertFalse(ENTRY.success(self.name, '{"passed":true,"checks":284,"scenarios":69}'))

    def test_console_requires_actual_dependency_log(self):
        self.fixture("console_display_entry")
        self.verify()
        path = self.path.parent / "settings-dependencies.log"
        path.write_bytes(b"PASS: settings dependencies (1 controls), independent consumers and saved-child restoration\n")
        self.rebind(path)
        with self.assertRaises(ValueError):
            self.verify()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(EntryEvidenceTests))
    report = {"schema": 1, "suite": "entry_evidence_policy", "scope": "synthetic packaging-policy tests only",
              "production_executed": False, "windows_executed": False, "game_tested": False,
              "tests_run": result.testsRun, "failures": len(result.failures), "errors": len(result.errors),
              "passed": result.wasSuccessful(), "sources": {
                  str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in (HERE / "entry_evidence.py", HERE / "entry_policy_fixtures.py", Path(__file__).resolve())}}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
