#!/usr/bin/env python3
"""Controlled tests of the production dependency worker; no Windows execution.

Run with --archives DIR to additionally extract and compare the exact official
Windows wheels. Network failures and child-import outcomes use explicit seams;
hashing, extraction, atomic publication, locks and status files are production.
"""
import argparse
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import signal
import stat
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock
import zipfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("runtime_download", ROOT / "engine" / "prepare_runtime.py")
D = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(D)
MANIFEST = D.load_manifest(ROOT / "engine" / "runtime-dependencies.json")
OFFICIAL_ARCHIVES = None


class Response:
    def __init__(self, body, row, *, error=None, after_read=None, url=None, status=200, length=None):
        self.body = io.BytesIO(body)
        self.row = row
        self.error = error
        self.after_read = after_read
        self.status = status
        self.url = row["url"] if url is None else url
        self.headers = {"Content-Length": str(row["bytes"] if length is None else length)}

    def geturl(self):
        return self.url

    def read(self, size):
        if self.error:
            raise self.error
        block = self.body.read(size)
        if self.after_read:
            self.after_read()
        return block

    def close(self):
        self.body.close()


class WorkerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ra2 runtime-依赖-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.profile = self.root / "profiles" / "测试光标"
        self.task = self.profile / ".preprocess" / "101-200-1"
        self.task.mkdir(parents=True)
        self.progress = D.Progress(self.task / "progress.ini", self.task / "cancel.flag")
        self.cache = self.root / "cache"
        self.cache.mkdir()
        self.row, self.body = self.fixture()
        self.archive = self.root / self.row["filename"]
        self.archive.write_bytes(self.body)

    def fixture(self, extra=(), *, metadata=None, extra_roots=()):
        row = copy.deepcopy(MANIFEST["packages"][0])
        members = [("numpy/__init__.py", b"__version__='2.2.6'\n"),
                   ("numpy.libs/native.dll", b"controlled fixture, not executable"),
                   ("numpy-2.2.6.dist-info/METADATA", metadata or b"Name: numpy\nVersion: 2.2.6\n"),
                   ("numpy-2.2.6.dist-info/LICENSE.txt", b"fixture license preserved\n")]
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in members + list(extra):
                archive.writestr(name, data)
        body = stream.getvalue()
        with zipfile.ZipFile(io.BytesIO(body)) as archive:
            row.update(bytes=len(body), sha256=hashlib.sha256(body).hexdigest(),
                       unpacked_bytes=sum(item.file_size for item in archive.infolist()),
                       members=len(archive.infolist()),
                       top_level=["numpy", "numpy.libs", "numpy-2.2.6.dist-info"] + list(extra_roots))
        return row, body

    def fetch(self, row, cache, progress):
        return self.archive

    def good_probe(self, python, row, paths, origin, progress):
        progress.checkpoint()
        return (origin / "numpy" / "__init__.py").is_file()

    def prepare(self, **kwargs):
        return D.prepare_components(self.root, Path(sys.executable), {"packages": [self.row]},
                                    self.progress, probe=kwargs.pop("probe", self.good_probe),
                                    fetch=kwargs.pop("fetch", self.fetch), **kwargs)

    def test_manifest_exact_known_versions_and_official_hosts(self):
        self.assertEqual(MANIFEST["python"]["version"], "3.12.10")
        self.assertEqual([row["id"] for row in MANIFEST["packages"]], ["numpy", "scipy", "opencv-python-headless"])
        for invalid in ("http://files.pythonhosted.org/a.whl", "https://evil.test/a.whl",
                        self.row["url"] + "?other=1", self.row["url"].replace("https://", "https://user:password@")):
            with self.subTest(url=invalid):
                row = dict(self.row, url=invalid)
                with self.assertRaises(D.DependencyError):
                    D.validate_record(row, "files.pythonhosted.org")

    def test_download_verified_then_cache_hit_never_contacts_network(self):
        opener = mock.Mock(side_effect=lambda row: Response(self.body, row))
        path = D.download_archive(self.row, self.cache, self.progress, open_url=opener)
        self.assertEqual(path.read_bytes(), self.body)
        second = D.download_archive(self.row, self.cache, self.progress, open_url=mock.Mock(side_effect=AssertionError("network")))
        self.assertEqual(path, second)
        self.assertEqual(opener.call_count, 1)
        self.assertFalse(list(self.cache.glob("*.part-*")))

    def test_download_rejects_truncation_hash_oversize_redirect_status_and_length(self):
        cases = [dict(body=self.body[:-1]), dict(body=b"x" * len(self.body)),
                 dict(body=self.body + b"x"), dict(body=self.body, url="https://evil.test/file"),
                 dict(body=self.body, status=206), dict(body=self.body, length=len(self.body) + 2)]
        for arguments in cases:
            with self.subTest(arguments={k: (len(v) if k == "body" else v) for k, v in arguments.items()}):
                with mock.patch.object(D.time, "sleep"):
                    with self.assertRaises(D.DependencyError):
                        D.download_archive(self.row, self.cache, self.progress,
                                           open_url=lambda row: Response(row=row, **arguments))
                self.assertFalse(list(self.cache.iterdir()))

    def test_interrupted_transfer_retries_cleanly(self):
        responses = [Response(self.body, self.row, error=TimeoutError("read stalled")), Response(self.body, self.row)]
        opener = mock.Mock(side_effect=lambda row: responses.pop(0))
        with mock.patch.object(D.time, "sleep"):
            result = D.download_archive(self.row, self.cache, self.progress, open_url=opener)
        self.assertEqual(result.read_bytes(), self.body)
        self.assertEqual(opener.call_count, 2)

    def test_corrupted_cache_is_replaced_without_executing_it(self):
        target = self.cache / (self.row["sha256"] + "-" + self.row["filename"])
        target.write_bytes(b"partial")
        result = D.download_archive(self.row, self.cache, self.progress,
                                    open_url=lambda row: Response(self.body, row))
        self.assertEqual(result, target)
        self.assertEqual(result.read_bytes(), self.body)

    def test_cancel_during_download_never_promotes_partial(self):
        with self.assertRaises(D.Cancelled):
            D.download_archive(self.row, self.cache, self.progress, open_url=lambda row:
                               Response(self.body, row, after_read=lambda: self.progress.cancel.touch()))
        self.assertFalse(list(self.cache.iterdir()))

    def test_cancel_before_cached_read_preserves_valid_cache(self):
        target = self.cache / (self.row["sha256"] + "-" + self.row["filename"])
        target.write_bytes(self.body)
        self.progress.cancel.touch()
        with self.assertRaises(D.Cancelled):
            D.download_archive(self.row, self.cache, self.progress)
        self.assertEqual(target.read_bytes(), self.body)

    def test_zip_extract_preserves_metadata_and_license(self):
        stage = self.root / "stage"
        stage.mkdir()
        receipt = D.extract_wheel(self.archive, self.row, stage, self.progress)
        self.assertEqual(receipt["members"], 4)
        self.assertEqual((stage / "numpy-2.2.6.dist-info/LICENSE.txt").read_bytes(), b"fixture license preserved\n")

    def test_zip_rejects_windows_path_traversal_aliases_and_devices_before_writing(self):
        paths = ["../escape", "/absolute", "C:/drive", "numpy\\backslash", "numpy/../escape",
                 "numpy/./ambiguous", "numpy//empty", "numpy/stream:evil", "numpy/trailing.",
                 "numpy/trailing ", "numpy/NUL.txt", "numpy/COM1", "numpy/LPT².txt", "numpy/a?b"]
        for number, name in enumerate(paths):
            with self.subTest(name=name):
                row, body = self.fixture([(name, b"bad")], extra_roots=[name.split("/")[0]])
                archive = self.root / f"bad{number}.whl"
                archive.write_bytes(body)
                stage = self.root / f"stage{number}"
                stage.mkdir()
                with self.assertRaises(D.DependencyError):
                    D.extract_wheel(archive, row, stage, self.progress)
                self.assertFalse(list(stage.iterdir()))

    def test_zip_rejects_case_collision_symlink_and_file_directory_conflict(self):
        symlink = zipfile.ZipInfo("numpy/link")
        symlink.create_system = 3
        symlink.external_attr = (stat.S_IFLNK | 0o777) << 16
        cases = [[("NUMPY/__INIT__.PY", b"duplicate")], [(symlink, b"../outside")], [("numpy", b"file")]]
        for number, extras in enumerate(cases):
            row, body = self.fixture(extras, extra_roots=["NUMPY"] if number == 0 else [])
            archive = self.root / f"conflict{number}.whl"
            archive.write_bytes(body)
            stage = self.root / f"collision{number}"
            stage.mkdir()
            with self.assertRaises(D.DependencyError):
                D.extract_wheel(archive, row, stage, self.progress)
            self.assertFalse(list(stage.iterdir()))

    def test_wrong_wheel_metadata_and_size_are_rejected(self):
        row, body = self.fixture(metadata=b"Name: numpy\nVersion: 9.9\n")
        bad = self.root / "badmeta.whl"
        bad.write_bytes(body)
        stage = self.root / "badmeta"
        stage.mkdir()
        with self.assertRaises(D.DependencyError):
            D.extract_wheel(bad, row, stage, self.progress)
        for key in ("unpacked_bytes", "members"):
            with self.assertRaises(D.DependencyError):
                D.extract_wheel(self.archive, dict(self.row, **{key: self.row[key] + 1}), stage, self.progress)

    def test_install_publish_reuse_and_damaged_component_repair(self):
        fetched = mock.Mock(side_effect=self.fetch)
        first = self.prepare(fetch=fetched)[0]
        self.assertEqual(fetched.call_count, 1)
        self.assertTrue((first / ".component.json").is_file())
        again = self.prepare(fetch=mock.Mock(side_effect=AssertionError("network")))
        self.assertEqual(again, [first])
        (first / "numpy/__init__.py").unlink()
        replacement = self.prepare()[0]
        self.assertNotEqual(first, replacement)
        self.assertTrue(first.is_dir())  # May still be loaded by another process.
        self.assertTrue((replacement / "numpy/__init__.py").is_file())
        self.assertFalse(list(first.parent.glob(".stage-*")))

    def test_legacy_healthy_component_uses_no_network(self):
        legacy = self.root / "runtime/Lib/site-packages"
        (legacy / "numpy").mkdir(parents=True)
        (legacy / "numpy/__init__.py").write_text("legacy")
        paths = self.prepare(fetch=mock.Mock(side_effect=AssertionError("network")))
        self.assertEqual(paths, [legacy])

    def test_managed_repairs_precede_legacy_for_every_selection_order(self):
        legacy = self.root / "legacy"
        numpy = self.root / "numpy-fixed"
        scipy = self.root / "scipy-fixed"
        for paths in ([legacy, scipy], [numpy, legacy, scipy], [legacy, numpy, legacy]):
            result = D.search_paths(list(paths), legacy)
            self.assertEqual(result[-1], legacy)
            self.assertEqual(len(result), len(set(result)))

    def test_failed_import_never_publishes_ready_component(self):
        with self.assertRaises(D.DependencyError):
            self.prepare(probe=lambda *args: False)
        self.assertFalse((self.root / "runtime/downloaded/components.json").exists())
        self.assertFalse(list((self.root / "runtime/downloaded/packages").iterdir()))

    def test_cancel_after_extraction_keeps_previous_index_unchanged(self):
        first = self.prepare()[0]
        (first / "numpy/__init__.py").unlink()
        index = self.root / "runtime/downloaded/components.json"
        before = index.read_bytes()
        def cancel_stage(python, row, paths, origin, progress):
            if origin.name.startswith(".stage-"):
                progress.cancel.touch()
                return True
            return False
        with self.assertRaises(D.Cancelled):
            self.prepare(probe=cancel_stage)
        self.assertEqual(index.read_bytes(), before)
        self.assertEqual(list(first.parent.iterdir()), [first])

    def test_index_write_failure_rolls_back_unpublished_new_directory(self):
        real = D.atomic_json
        def broken_index(path, value):
            if path.name == "components.json":
                raise PermissionError("controlled index write failure")
            return real(path, value)
        with mock.patch.object(D, "atomic_json", side_effect=broken_index):
            with self.assertRaises(PermissionError):
                self.prepare()
        self.assertFalse(list((self.root / "runtime/downloaded/packages").iterdir()))

    def test_malformed_per_component_index_is_repairable(self):
        managed = self.root / "runtime/downloaded"
        managed.mkdir(parents=True)
        for malformed in (None, [], "invalid", 4):
            (managed / "components.json").write_text(json.dumps({"schema_version": 1, "components": {"numpy": malformed}}))
            self.assertTrue((self.prepare()[0] / ".component.json").is_file())

    def test_unindexed_incomplete_stage_is_never_used(self):
        stage = self.root / "runtime/downloaded/packages/.stage-abandoned"
        (stage / "numpy").mkdir(parents=True)
        (stage / "numpy/__init__.py").write_text("partial")
        path = self.prepare()[0]
        self.assertNotEqual(path, stage)
        self.assertTrue((path / ".component.json").is_file())

    def test_task_paths_are_unique_and_old_cancel_survives_new_task(self):
        self.assertEqual(D.validate_task_paths(self.profile, self.progress.status, self.progress.cancel), self.profile.resolve())
        second = self.task.with_name("102-300-1")
        second.mkdir()
        next_progress = D.Progress(second / "progress.ini", second / "cancel.flag")
        self.progress.cancel.touch()
        next_progress.write("dependency_wait", "新任务等待旧任务退出")
        next_progress.checkpoint()
        with self.assertRaises(D.Cancelled):
            self.progress.checkpoint()
        self.assertTrue(self.progress.cancel.exists())
        with self.assertRaises(D.DependencyError):
            D.validate_task_paths(self.profile, self.progress.status, next_progress.cancel)

    def test_profile_lock_is_exclusive_and_released_after_failure(self):
        lock_path = self.profile / ".preprocess.lock"
        with D.InstallLock(lock_path, self.progress):
            with self.assertRaises(D.DependencyError):
                with D.InstallLock(lock_path, self.progress, timeout=0.1):
                    self.fail("second writer admitted")
        with self.assertRaises(RuntimeError):
            with D.InstallLock(lock_path, self.progress):
                raise RuntimeError("controlled worker failure")
        with D.InstallLock(lock_path, self.progress, timeout=0.1):
            pass

    def test_waiting_task_cancellation_does_not_release_owner_lock(self):
        lock_path = self.profile / ".preprocess.lock"
        second = self.task.with_name("102-300-2")
        second.mkdir()
        waiting = D.Progress(second / "progress.ini", second / "cancel.flag")
        with D.InstallLock(lock_path, self.progress):
            waiting.cancel.touch()
            with self.assertRaises(D.Cancelled):
                with D.InstallLock(lock_path, waiting):
                    self.fail("cancelled task admitted")
            with self.assertRaises(D.DependencyError):
                with D.InstallLock(lock_path, self.progress, timeout=0.1):
                    self.fail("owner lock lost")

    def test_status_is_utf16_and_error_kind_is_preserved(self):
        self.progress.write("error", "下载失败\n请重试", error_kind="dependency")
        raw = self.progress.status.read_bytes()
        self.assertTrue(raw.startswith(b"\xff\xfe"))
        text = raw.decode("utf-16")
        self.assertIn("Message=下载失败 请重试", text)
        self.assertIn("ErrorKind=dependency", text)

    def test_probe_direct_child_exit_does_not_wait_for_inherited_output_handle(self):
        pid_file = self.root / "descendant.pid"
        code = ("import subprocess,sys,pathlib; "
                "child=subprocess.Popen([sys.executable,'-c','import time; time.sleep(5)']); "
                f"pathlib.Path({str(pid_file)!r}).write_text(str(child.pid)); "
                "print('RA2_DEPENDENCY_OK')")
        started = time.monotonic()
        try:
            with mock.patch.object(D, "PROBE_CODE", code):
                self.assertTrue(D.run_probe(Path(sys.executable), self.row, [], self.root, self.progress))
            self.assertLess(time.monotonic() - started, 2)
        finally:
            if pid_file.exists():
                with contextlib.suppress(ProcessLookupError):
                    os.kill(int(pid_file.read_text()), signal.SIGTERM)

    def test_probe_cancellation_ends_its_child_promptly(self):
        timer = threading.Timer(0.25, self.progress.cancel.touch)
        started = time.monotonic()
        timer.start()
        try:
            with mock.patch.object(D, "PROBE_CODE", "import time; time.sleep(30)"):
                with self.assertRaises(D.Cancelled):
                    D.run_probe(Path(sys.executable), self.row, [], self.root, self.progress)
            self.assertLess(time.monotonic() - started, 2)
        finally:
            timer.cancel()
            timer.join()

    def test_real_official_windows_wheels_extract_without_executing(self):
        if OFFICIAL_ARCHIVES is None:
            self.skipTest("official archive directory not supplied")
        for row in MANIFEST["packages"]:
            with self.subTest(package=row["id"]):
                stage = self.root / ("official-" + row["id"])
                stage.mkdir()
                source = OFFICIAL_ARCHIVES / row["filename"]
                details = D.extract_wheel(source, row, stage, self.progress)
                with zipfile.ZipFile(source) as archive:
                    for member in archive.infolist():
                        if not member.is_dir():
                            self.assertEqual((stage / member.filename).read_bytes(), archive.read(member))
                self.assertEqual(details["unpacked_bytes"], row["unpacked_bytes"])
                self.assertTrue(any(path.is_file() for path in stage.rglob("*LICENSE*")))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--archives", type=Path)
    options, extra = parser.parse_known_args()
    OFFICIAL_ARCHIVES = options.archives
    unittest.main(argv=[sys.argv[0]] + extra, verbosity=2)
