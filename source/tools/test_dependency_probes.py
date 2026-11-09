#!/usr/bin/env python3
"""Portable checks of complete-cache probe batching; no dependency installation.

The real child scripts execute against synthetic modules behind a counting
Popen model. No scientific package, child process, download or installer runs.
"""
import argparse
from collections import Counter
import contextlib
import copy
import hashlib
import importlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("dependency_probes_worker", ROOT / "engine/prepare_runtime.py")
D = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(D)
MANIFEST = D.load_manifest(ROOT / "engine/runtime-dependencies.json")
METRICS = {}
ORIGINAL_PROBE_SHA256 = "6aaf8cc280cfa46b92936587ee055261f8fe25d5a251c9f0731d174ac1cb7206"


class RepairReached(Exception):
    """The unchanged repair branch reached its injected, non-installing fetch."""


class Array:
    def __init__(self, *, shape=(), total=6, distance=2):
        self.shape, self.total, self.distance = shape, total, distance

    def sum(self):
        return self.total

    def __getitem__(self, index):
        return self.distance


class ProbeProcess:
    def __init__(self, returncode):
        self.returncode = returncode

    def poll(self):
        return self.returncode

    def kill(self):
        raise AssertionError("a completed synthetic probe must not be killed")

    def wait(self, timeout=None):
        return self.returncode


class ScientificProbes:
    """Execute original assertions with synthetic version/origin/function facts."""
    def __init__(self, origins, *, wrong_version=None, wrong_origin=None, broken=None, exit_zero=None):
        self.origins = origins
        self.wrong_version, self.wrong_origin = wrong_version, wrong_origin
        self.broken, self.exit_zero = broken, exit_zero
        self.launches, self.module_checks, self.outputs = [], [], []
        self.functions = Counter()

    def feature(self, name, good, bad=False):
        self.functions[name] += 1
        if self.exit_zero == name:
            raise SystemExit(0)
        return bad if self.broken == name else good

    def modules(self):
        modules = {}
        for row in MANIFEST["packages"]:
            name = row["module"]
            module = types.ModuleType(name)
            module.__version__ = "wrong" if self.wrong_version == name else row["module_version"]
            folder = self.origins[name]
            module.__file__ = str((folder.parent / "outside" if self.wrong_origin == name else folder) / name / "__init__.py")
            module.__path__ = [str(folder / name)]
            modules[name] = module
        np = modules["numpy"]
        np.uint8 = "synthetic uint8"
        np.arange = lambda *a, **k: Array(total=self.feature("numpy_arange", 6, 7))
        np.eye = lambda *a: Array(shape=(2, 2))
        np.ones = lambda *a: Array(shape=(2,))
        np.linalg = types.SimpleNamespace(solve=lambda *a: self.feature("numpy_solve", True, False))
        np.allclose = lambda left, right: bool(left)
        np.array = lambda *a, **k: Array(shape=(2, 2))
        np.zeros = lambda shape, **k: Array(shape=shape)
        spatial = types.ModuleType("scipy.spatial")
        spatial.cKDTree = lambda *a: types.SimpleNamespace(query=lambda *a: (0, self.feature("scipy_tree", 0, 1)))
        ndimage = types.ModuleType("scipy.ndimage")
        ndimage.distance_transform_edt = lambda *a: Array(distance=self.feature("scipy_distance", 2, 0))
        modules.update({"scipy.spatial": spatial, "scipy.ndimage": ndimage})
        cv = modules["cv2"]
        cv.DISOPTICAL_FLOW_PRESET_MEDIUM, cv.COLOR_RGB2GRAY = 1, 2
        cv.DISOpticalFlow_create = lambda *a: self.feature("cv_dis", types.SimpleNamespace(calc=lambda: None), object())
        cv.cvtColor = lambda image, mode: self.feature("cv_color", Array(shape=(4, 4)), None)
        cv.resize = lambda image, shape: Array(shape=self.feature("cv_resize", shape if image is not None else (), (1, 1)))
        cv.imencode = lambda *a: (self.feature("cv_encode", True, False), b"synthetic png")
        return modules

    def popen(self, arguments, **kwargs):
        assert arguments[1:4] == ["-I", "-S", "-c"]
        assert kwargs["stdin"] == D.subprocess.DEVNULL
        assert kwargs["stderr"] == D.subprocess.STDOUT
        assert "shell" not in kwargs
        self.launches.append(list(arguments))
        output = io.StringIO()
        real_import = importlib.import_module

        def observe_import(name, *args):
            if name in ("numpy", "scipy", "cv2"):
                self.module_checks.append(name)
            return real_import(name, *args)

        before = sys.path[:]
        returncode = 0
        try:
            with mock.patch.dict(sys.modules, self.modules()), mock.patch.object(importlib, "import_module", observe_import), \
                    mock.patch.object(sys, "argv", ["-c", arguments[5]]), contextlib.redirect_stdout(output):
                try:
                    exec(arguments[4], {"__name__": "__main__"})
                except SystemExit as error:
                    returncode = error.code or 0
                except Exception:
                    returncode = 1
        finally:
            sys.path[:] = before
        text = output.getvalue()
        self.outputs.append(text)
        kwargs["stdout"].write(text.encode())
        return ProbeProcess(returncode)


class BatchProbeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ra2-probe-cache-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = copy.deepcopy(MANIFEST)
        self.progress = D.Progress(self.root / "progress.ini", self.root / "cancel.flag")
        self.python = Path("synthetic-python-not-executed")
        self.components = self.root / "runtime/downloaded/packages"
        self.components.mkdir(parents=True)
        self.index_path = self.components.parent / "components.json"
        self.folders, self.origins = [], {}
        index = {"schema_version": 1, "components": {}}
        for number, row in enumerate(self.manifest["packages"], 1):
            folder = self.components / (row["id"] + "-" + row["sha256"][:12] + "-" + f"{number:032x}")
            folder.mkdir()
            D.atomic_json(folder / ".component.json", {key: row[key] for key in ("id", "version", "sha256")})
            index["components"][row["id"]] = {"directory": folder.name, "version": row["version"], "sha256": row["sha256"]}
            self.folders.append(folder)
            self.origins[row["module"]] = folder
        D.atomic_json(self.index_path, index)
        self.fetch = mock.Mock(side_effect=RepairReached("no download or installation in this test"))

    def prepare(self, **kwargs):
        return D.prepare_components(self.root, self.python, self.manifest, self.progress, fetch=self.fetch, **kwargs)

    def checks(self):
        return [(row, self.folders[:number], self.folders[number - 1])
                for number, row in enumerate(self.manifest["packages"], 1)]

    def test_original_probe_assertions_unchanged(self):
        self.assertEqual(hashlib.sha256(D.PROBE_CODE.encode()).hexdigest(), ORIGINAL_PROBE_SHA256)

    def test_warm_eight_batches_keep_twenty_four_module_checks(self):
        original_index = self.index_path.read_bytes()
        legacy = ScientificProbes(self.origins)
        with mock.patch.object(D.subprocess, "Popen", legacy.popen):
            for _ in range(8):
                self.assertEqual(self.prepare(probe=lambda *args: D.run_probe(*args)), self.folders)
        current = ScientificProbes(self.origins)
        with mock.patch.object(D.subprocess, "Popen", current.popen):
            for _ in range(8):
                self.assertEqual(self.prepare(), self.folders)
        self.assertEqual(len(legacy.launches), 24)
        self.assertEqual(len(current.launches), 8)
        self.assertEqual(legacy.module_checks, ["numpy", "scipy", "cv2"] * 8)
        self.assertEqual(current.module_checks, legacy.module_checks)
        self.assertEqual(current.functions, legacy.functions)
        self.assertEqual(current.functions["numpy_arange"], 24)
        for feature in ("numpy_solve", "scipy_tree", "scipy_distance", "cv_dis", "cv_color", "cv_resize", "cv_encode"):
            self.assertEqual(current.functions[feature], 8)
        for arguments in current.launches:
            probe, payloads = json.loads(arguments[5])
            self.assertEqual(probe, D.PROBE_CODE)
            for number, payload in enumerate(payloads, 1):
                self.assertEqual(payload[0], [str(p) for p in self.folders[:number]])
                self.assertEqual(payload[3], str(self.folders[number - 1]))
        self.fetch.assert_not_called()
        self.assertEqual(self.index_path.read_bytes(), original_index)
        self.assertFalse(list(self.components.glob(".stage-*")))
        METRICS["warm"] = {"batches": 8, "before_probe_processes": 24, "after_probe_processes": 8,
                           "before_module_checks": 24, "after_module_checks": 24,
                           "functional_checks": dict(current.functions), "downloads": 0, "installations": 0,
                           "processes_modeled": True, "original_probe_code_executed": True}

    def test_injected_single_probe_never_starts_implicit_batch(self):
        probe = mock.Mock(return_value=True)
        with mock.patch.object(D.subprocess, "Popen", side_effect=AssertionError("unexpected real child")), \
                mock.patch.object(D, "run_batch_probe", side_effect=AssertionError("implicit batch")):
            self.assertEqual(self.prepare(probe=probe), self.folders)
        self.assertEqual(probe.call_count, 3)
        self.fetch.assert_not_called()

    def test_missing_marker_skips_batch_and_reaches_unchanged_repair(self):
        (self.folders[1] / ".component.json").unlink()
        probe, batch = mock.Mock(return_value=True), mock.Mock(return_value=True)
        with mock.patch.object(D.subprocess, "Popen", side_effect=AssertionError("unexpected child")):
            with self.assertRaises(RepairReached):
                self.prepare(probe=probe, batch_probe=batch)
        batch.assert_not_called()
        self.assertEqual([call.args[1]["id"] for call in probe.call_args_list], ["numpy"])
        self.assertEqual(self.fetch.call_args.args[0]["id"], "scipy")

    def test_bad_index_and_marker_use_original_individual_path(self):
        for broken in ("index_version", "marker_sha256"):
            with self.subTest(broken=broken):
                index = D.read_small_json(self.index_path)
                marker_path = self.folders[2] / ".component.json"
                marker = D.read_small_json(marker_path)
                if broken == "index_version":
                    changed = copy.deepcopy(index)
                    changed["components"]["opencv-python-headless"]["version"] = "wrong"
                    D.atomic_json(self.index_path, changed)
                else:
                    D.atomic_json(marker_path, dict(marker, sha256="0" * 64))
                probe, batch = mock.Mock(return_value=True), mock.Mock(return_value=True)
                with self.assertRaises(RepairReached):
                    self.prepare(probe=probe, batch_probe=batch)
                batch.assert_not_called()
                self.assertEqual([call.args[1]["module"] for call in probe.call_args_list], ["numpy", "scipy"])
                self.assertEqual(self.fetch.call_args.args[0]["module"], "cv2")
                D.atomic_json(self.index_path, index)
                D.atomic_json(marker_path, marker)

    def test_failed_batch_rechecks_every_component_individually(self):
        probe, batch = mock.Mock(return_value=True), mock.Mock(return_value=False)
        self.assertEqual(self.prepare(probe=probe, batch_probe=batch), self.folders)
        self.assertEqual(batch.call_count, 1)
        self.assertEqual([call.args[1]["module"] for call in probe.call_args_list], ["numpy", "scipy", "cv2"])
        self.fetch.assert_not_called()

    def test_failed_batch_retains_individual_failure_repair_order(self):
        probe = mock.Mock(side_effect=lambda python, row, *rest: row["module"] != "scipy")
        with self.assertRaises(RepairReached):
            self.prepare(probe=probe, batch_probe=mock.Mock(return_value=False))
        self.assertEqual([call.args[1]["module"] for call in probe.call_args_list], ["numpy", "scipy"])
        self.assertEqual(self.fetch.call_args.args[0]["module"], "scipy")

    def test_batch_spawn_error_falls_back_but_cancellation_propagates(self):
        probe = mock.Mock(return_value=True)
        self.assertEqual(self.prepare(probe=probe, batch_probe=mock.Mock(side_effect=OSError("synthetic spawn failure"))), self.folders)
        self.assertEqual(probe.call_count, 3)
        probe.reset_mock()
        with self.assertRaises(D.Cancelled):
            self.prepare(probe=probe, batch_probe=mock.Mock(side_effect=D.Cancelled("synthetic cancel")))
        probe.assert_not_called()
        self.fetch.assert_not_called()

    def test_cancel_checkpoint_prevents_batch(self):
        self.progress.cancel.touch()
        probe, batch = mock.Mock(return_value=True), mock.Mock(return_value=True)
        with self.assertRaises(D.Cancelled):
            self.prepare(probe=probe, batch_probe=batch)
        probe.assert_not_called()
        batch.assert_not_called()
        self.fetch.assert_not_called()

    def test_each_module_version_and_origin_are_still_checked(self):
        for key in ("wrong_version", "wrong_origin"):
            for number, module in enumerate(("numpy", "scipy", "cv2"), 1):
                with self.subTest(key=key, module=module):
                    probes = ScientificProbes(self.origins, **{key: module})
                    with mock.patch.object(D.subprocess, "Popen", probes.popen):
                        self.assertFalse(D.run_batch_probe(self.python, self.checks(), self.progress))
                    self.assertEqual(len(probes.launches), 1)
                    self.assertEqual(probes.module_checks, ["numpy", "scipy", "cv2"][:number])
                    self.assertNotIn("RA2_DEPENDENCIES_OK", probes.outputs[0])

    def test_every_original_functional_guard_rejects_failure(self):
        for feature in ("numpy_arange", "numpy_solve", "scipy_tree", "scipy_distance", "cv_dis", "cv_color", "cv_resize", "cv_encode"):
            with self.subTest(feature=feature):
                probes = ScientificProbes(self.origins, broken=feature)
                with mock.patch.object(D.subprocess, "Popen", probes.popen):
                    self.assertFalse(D.run_batch_probe(self.python, self.checks(), self.progress))
                self.assertNotIn("RA2_DEPENDENCIES_OK", probes.outputs[0])

    def test_early_zero_exit_cannot_reuse_an_earlier_success_marker(self):
        probes = ScientificProbes(self.origins, exit_zero="scipy_tree")
        with mock.patch.object(D.subprocess, "Popen", probes.popen):
            self.assertFalse(D.run_batch_probe(self.python, self.checks(), self.progress))
        self.assertIn("RA2_DEPENDENCY_OK", probes.outputs[0])
        self.assertNotIn("RA2_DEPENDENCIES_OK", probes.outputs[0])
        self.assertEqual(probes.module_checks, ["numpy", "scipy"])

    def test_partial_manifest_preserves_injected_probe_compatibility(self):
        self.manifest["packages"] = self.manifest["packages"][:1]
        probe, batch = mock.Mock(return_value=True), mock.Mock(return_value=True)
        self.assertEqual(self.prepare(probe=probe, batch_probe=batch), self.folders[:1])
        self.assertEqual(probe.call_count, 1)
        batch.assert_not_called()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(BatchProbeTests))
    log = out / "tests.log"
    log.write_text(stream.getvalue())
    paths = ["engine/prepare_runtime.py", "engine/runtime-dependencies.json", "tools/test_dependency_probes.py"]
    receipt = {"passed": result.wasSuccessful(), "tests_run": result.testsRun,
               "failures": len(result.failures), "errors": len(result.errors),
               "metrics": METRICS, "log_path": str(log), "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest(),
               "source_sha256": {"source/" + p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths},
               "command": [sys.executable, str(Path(__file__).resolve()), "--out", str(out)],
               "real_probe_children_started": 0, "downloads": 0, "installations": 0,
               "windows_executed": False, "native_extensions_executed": False,
               "boundary": "Actual prepare/cache checks and original PROBE_CODE plus batch script; synthetic legal metadata, synthetic scientific modules and counting Popen. No subprocess, network or package installation in the test."}
    (out / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n")
    print(stream.getvalue(), end="")
    print(json.dumps({key: receipt[key] for key in ("passed", "tests_run", "failures", "errors", "metrics")}, ensure_ascii=False))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
