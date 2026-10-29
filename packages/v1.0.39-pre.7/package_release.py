#!/usr/bin/env python3
"""Package the exact eight-job x64 frontend prerelease, with separate sources."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
VERSION, FILE_VERSION = "1.0.39-pre.7", "1.0.39.7"
PACKAGE = "packages/v" + VERSION
REPOSITORY = "ARISAKAHIKARI/RA2-Toolbox-Build"
BINARIES = ("RA2Toolbox.exe", "GameRuntime.dll", "GameRuntimeHost32.exe", "RA2NeuralRenderer.exe")
ARTIFACT_FILES = set(BINARIES) | {"WINDOWS_BUILD_ARTIFACTS.json", "BUILD_TARGET.json", "X64_CI_BUILD.json", "NEURAL-NOTICE.txt", "LICENSE-NVIDIA-RTX-SDK.txt", "LICENSE-NVIDIA-DLSS-BASE.txt"}
GROUPS = {
    ".github/workflows/validate-render-feedback-fixes.yml": {"portable", "windows-x86", "vulkan-compute"},
    ".github/workflows/validate-x64-render.yml": {"windows-mixed-architecture", "vulkan-guides"},
    ".github/workflows/validate-frontend.yml": {"windows-controls (x86-windows-gnu)", "windows-controls (x86_64-windows-gnu)", "portable-gestures"},
}
FORBIDDEN = {".exe", ".dll", ".pdb", ".zip", ".o", ".obj", ".lib", ".a", ".so", ".dylib", ".pyc", ".pyo", ".whl", ".7z", ".rar", ".dmp", ".dump", ".sqlite", ".db"}
ROOT_SOURCES = {"AGENTS.md", "README.md", "LICENSE-GPL", "THIRD_PARTY_NOTICES.md", ".gitattributes"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def positive(value, label):
    require(not isinstance(value, bool) and (isinstance(value, int) or isinstance(value, str) and value.isdecimal()), label + " must be an integer")
    require(int(value) > 0, label + " must be positive")
    return int(value)


def records(rows, expected, label):
    require(isinstance(rows, list) and len(rows) == len(expected) and all(isinstance(row, dict) and isinstance(row.get("name"), str) for row in rows), label + " records invalid")
    by_name = {row["name"]: row for row in rows}
    require(len(by_name) == len(rows) and set(by_name) == set(expected), label + " membership differs")
    return by_name


def validate_ci(ci, commit):
    require(re.fullmatch(r"[0-9a-f]{40}", commit) is not None and ci.get("commit") == commit and ci.get("conclusion") == "success", "Final successful product commit is required")
    runs, jobs = ci.get("runs"), ci.get("jobs")
    require(isinstance(runs, list) and len(runs) == 3 and isinstance(jobs, list) and len(jobs) == 8, "Three runs and eight jobs required")
    by_run = {}
    for run in runs:
        number = positive(run.get("run_id"), "Run ID")
        require(positive(run.get("id"), "Run object ID") == number and number not in by_run, "Run IDs disagree or repeat")
        require(run.get("head_sha") == commit and run.get("status") == "completed" and run.get("conclusion") == "success", "Mixed or unsuccessful CI run")
        require(run.get("url") == f"https://github.com/{REPOSITORY}/actions/runs/{number}", "CI run URL differs")
        by_run[number] = {"run": run, "names": set()}
    require(len({positive(job.get("id"), "Job ID") for job in jobs}) == 8, "Repeated CI jobs")
    for job in jobs:
        number = positive(job.get("run_id"), "Job run ID")
        require(number in by_run and job.get("status") == "completed" and job.get("conclusion") == "success", "A required job did not succeed")
        require(job.get("head_sha", commit) == commit, "Job commit differs")
        by_run[number]["names"].add(job.get("name"))
    require({frozenset(row["names"]) for row in by_run.values()} == {frozenset(names) for names in GROUPS.values()}, "Required jobs were omitted or mixed between runs")
    for row in by_run.values():
        path = next(path for path, names in GROUPS.items() if names == row["names"])
        require(row["run"].get("path", path) == path, "CI workflow path differs")
    x64 = next(number for number, row in by_run.items() if row["names"] == GROUPS[".github/workflows/validate-x64-render.yml"])
    require(positive(ci.get("run_id"), "Primary run") == x64 and ci.get("url") == f"https://github.com/{REPOSITORY}/actions/runs/{x64}", "Primary CI is not the x64 run")
    for key, value in {"probe_status": 7, "available": False, "runtime_installed": False, "hardware_inference_tested": False, "model_executed": False}.items():
        require(ci.get("neural", {}).get(key) == value, "Missing explicit NoRuntime evidence: " + key)
    require(ci.get("actual_game_tested") is False, "Controlled tests cannot certify a live game")
    artifact = ci.get("artifact", {})
    require(artifact.get("name") == "RA2-x64-render-experiment" and positive(artifact.get("run_id"), "Artifact run") == x64 and artifact.get("head_sha") == commit, "Wrong artifact identity")
    positive(artifact.get("artifact_id"), "Artifact ID")
    records(artifact.get("files"), ARTIFACT_FILES, "Artifact")
    return x64


def safe_source(path):
    parts = PurePosixPath(path).parts
    require(parts and not PurePosixPath(path).is_absolute() and not any(part in {".", "..", ".git", "__pycache__", "neural-runtime"} for part in parts) and "\\" not in path and "\n" not in path and "\r" not in path, "Unsafe source path: " + repr(path))
    require(PurePosixPath(path).suffix.lower() not in FORBIDDEN, "Binary/archive/dump in sources: " + path)


def verify_sources(files):
    required = ROOT_SOURCES | set(GROUPS) | {PACKAGE + "/package_release.py", PACKAGE + "/README.txt", PACKAGE + "/RELEASE_NOTES.md", "source/shared/source/release_version.h", "source/BUILD_SOURCE_MANIFEST.json", "source/tools/verify_windows_outputs.py", "source/tools/pe32_image.py", "source/shared/source/page_wheel.h", "source/suite/source/frontend_panes.h", "source/assist/tests/frontend_controls_windows.cpp", "source/assist/tests/frontend_panes_windows.cpp", "source/third_party/xbrz/License.txt", "source/neural_renderer/source/vulkan_ngx.h"}
    require(required <= files.keys(), "Missing corresponding sources: " + ", ".join(sorted(required - files.keys())))
    require(files[PACKAGE + "/package_release.py"][0] == Path(__file__).read_bytes(), "Running packager differs from source commit")
    header = files["source/shared/source/release_version.h"][0].decode("utf-8")
    require(f'#define RA2_RELEASE_VERSION "{FILE_VERSION}"' in header and f'L"{VERSION}"' in header, "Source release version differs")


def source_from_archive(args, commit):
    tree, provenance = load(args.remote_tree), load(args.source_provenance)
    require(tree.get("truncated") is False and provenance.get("commit") == commit and provenance.get("tree_sha") == tree.get("sha"), "Source commit/tree identity differs")
    raw = args.source_zip.read_bytes()
    require(provenance.get("archive_bytes", len(raw)) == len(raw) and provenance.get("archive_sha256", sha(raw)) == sha(raw), "Source archive digest differs")
    remote = {row["path"]: row for row in tree["tree"] if row["type"] == "blob"}
    files = {}
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        require(archive.testzip() is None, "Source CRC failed")
        for item in archive.infolist():
            if item.is_dir():
                continue
            name = item.filename.removeprefix("RA2-Toolbox-source/")
            safe_source(name)
            require(name not in files and name in remote and remote[name]["mode"] in {"100644", "100755"}, "Unknown, repeated or non-regular source: " + name)
            data = archive.read(item)
            require(blob(data) == remote[name]["sha"], "Source Git blob differs: " + name)
            files[name] = (data, int(remote[name]["mode"], 8))
    require(len(files) == provenance.get("verified_included_blobs"), "Source blob count differs")
    live_product = {name for name in remote if name.startswith("source/") and not name.startswith("source/verification/")}
    require(live_product <= files.keys(), "Current product source was omitted")
    verify_sources(files)
    provenance = dict(provenance, archive_bytes=len(raw), archive_sha256=sha(raw), verified_git_blobs=len(files))
    return raw, files, provenance


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def source_from_git(commit):
    require(git("cat-file", "-t", commit).strip() == b"commit", "Product Git commit is unavailable")
    files = {}
    for record in git("ls-tree", "-r", "-z", "--full-tree", commit).split(b"\0"):
        if not record:
            continue
        metadata, raw_path = record.split(b"\t", 1)
        mode, kind, identity = metadata.decode("ascii").split()
        name = raw_path.decode("utf-8")
        if not (name in ROOT_SOURCES or name in GROUPS or name.startswith(("source/", "docs/", PACKAGE + "/"))) or name.startswith("source/verification/"):
            continue
        if PurePosixPath(name).suffix.lower() in FORBIDDEN or "__pycache__" in PurePosixPath(name).parts:
            continue
        safe_source(name)
        require(mode in {"100644", "100755"} and kind == "blob", "Non-regular source: " + name)
        data = git("cat-file", "blob", identity)
        require(blob(data) == identity, "Git source bytes changed: " + name)
        files[name] = (data, int(mode, 8))
    verify_sources(files)
    raw = make_zip(files)
    return raw, files, {"commit": commit, "tree_sha": git("rev-parse", commit + "^{tree}").decode().strip(), "verified_included_blobs": len(files), "verified_git_blobs": len(files), "historical_nonproduct_files_omitted": True, "archive_bytes": len(raw), "archive_sha256": sha(raw)}


def validate_binaries(directory, ci, sources):
    artifact = ci["artifact"]; path = Path(artifact.get("path", ""))
    require(path.is_absolute() and path.is_file(), "Original CI artifact ZIP is required")
    raw = path.read_bytes()
    require(len(raw) == positive(artifact.get("bytes"), "Artifact bytes") and sha(raw) == artifact.get("sha256"), "Original artifact ZIP bytes/digest differ")
    declared = records(artifact["files"], ARTIFACT_FILES, "Artifact")
    files = {name: (directory / name).read_bytes() for name in ARTIFACT_FILES}
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        require(archive.testzip() is None and len(archive.namelist()) == 10 and set(archive.namelist()) == ARTIFACT_FILES, "Expected ten flat artifact entries with valid CRC")
        for name, data in files.items():
            require(declared[name].get("bytes") == len(data) and declared[name].get("sha256") == sha(data) and archive.read(name) == data, "Artifact file binding differs: " + name)
    build = json.loads(files["X64_CI_BUILD.json"])
    require(build.get("commit") == ci["commit"] and positive(build.get("run_id"), "Build run") == positive(ci["run_id"], "CI run"), "Build commit/run differ")
    for key, value in {"host": "x64", "game_runtime": "x86", "broker": "x86", "neural_helper": "x64", "actual_game_tested": False, "hardware_dlss5_tested": False, "missing_runtime_probe": True}.items():
        require(build.get(key) == value, "Build declaration differs: " + key)
    for name, record in records(build.get("files"), ARTIFACT_FILES - {"X64_CI_BUILD.json"}, "Build").items():
        require(record.get("bytes") == len(files[name]) and record.get("sha256") == sha(files[name]), "Build file digest differs: " + name)
    target, metadata = json.loads(files["BUILD_TARGET.json"]), json.loads(files["WINDOWS_BUILD_ARTIFACTS.json"])
    for key, value in {"host": "x64", "runtime": "x86", "broker": "x86", "assist_protocol": 80, "runtime_protocol": 4, "game_tested": False}.items():
        require(target.get(key) == value, "Target declaration differs: " + key)
    require(metadata.get("file_version") == FILE_VERSION and metadata.get("host_architecture") == "x64" and metadata.get("runtime_architecture") == "x86" and metadata.get("neural_helper_verified") is True, "Windows PE metadata version/profile differs")
    require(all(metadata.get(key) is False for key in ("runtime_tested", "multiplayer_tested", "neural_runtime_tested")), "PE metadata must retain execution limits")
    for name in ("verify_windows_outputs.py", "pe32_image.py"):
        require((ROOT / "source/tools" / name).read_bytes() == sources["source/tools/" + name][0], "Running PE verifier differs from committed source")
    sys.path.insert(0, str(ROOT / "source/tools"))
    import verify_windows_outputs
    with tempfile.TemporaryDirectory(prefix="ra2-pre7-pe-") as temporary:
        stage = Path(temporary)
        for name in BINARIES:
            (stage / name).write_bytes(files[name])
        (stage / "BUILD_SOURCE_MANIFEST.json").write_bytes(sources["source/BUILD_SOURCE_MANIFEST.json"][0])
        actual, errors = verify_windows_outputs.verify(stage, host_arch="x64", neural_helper=True)
    require(not errors, "Four-component PE verification failed: " + "; ".join(errors))
    actual = json.loads(json.dumps(actual))
    require(actual == metadata.get("verified"), "Actual PE bytes differ from CI PE metadata")
    for name in ("RA2Toolbox.exe", "GameRuntime.dll"):
        require(actual[name].get("assist_protocol") == 80 and actual[name].get("assist_shared_bytes") == 4169840, "Assist ABI differs: " + name)
    for name, source in (("NEURAL-NOTICE.txt", "NOTICE.txt"), ("LICENSE-NVIDIA-RTX-SDK.txt", "licenses/NVIDIA-RTX-SDK-LICENSE.txt"), ("LICENSE-NVIDIA-DLSS-BASE.txt", "licenses/NVIDIA-DLSS-BASE-LICENSE.txt")):
        require(files[name].decode("utf-8-sig").replace("\r\n", "\n") == sources["source/neural_renderer/" + source][0].decode("utf-8-sig").replace("\r\n", "\n"), "NVIDIA notice differs from source: " + name)
    return files, actual


def make_zip(files):
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, (data, mode) in sorted(files.items()):
            require(not PurePosixPath(name).is_absolute() and ".." not in PurePosixPath(name).parts and "\\" not in name, "Unsafe ZIP entry")
            item = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0)); item.create_system = 3; item.compress_type = zipfile.ZIP_DEFLATED; item.external_attr = mode << 16
            archive.writestr(item, data)
    raw = output.getvalue()
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        require(archive.testzip() is None and len(archive.namelist()) == len(files) and set(archive.namelist()) == set(files), "Generated ZIP CRC/membership differs")
        require(all(archive.read(name) == data for name, (data, _) in files.items()), "Generated ZIP payload differs")
    return raw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binaries", type=Path, required=True)
    parser.add_argument("--ci", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--source-zip", type=Path)
    parser.add_argument("--source-provenance", type=Path)
    parser.add_argument("--remote-tree", type=Path)
    args = parser.parse_args()
    ci = load(args.ci); validate_ci(ci, args.commit)
    supplied = (args.source_zip, args.source_provenance, args.remote_tree)
    require(all(supplied) or not any(supplied), "Source ZIP, provenance and remote tree must be provided together")
    source_zip, sources, provenance = source_from_archive(args, args.commit) if all(supplied) else source_from_git(args.commit)
    files, actual = validate_binaries(args.binaries, ci, sources)
    binaries = {name: {"bytes": len(files[name]), "sha256": sha(files[name])} for name in BINARIES}
    limits = {"live_game_verified": False, "multiplayer_verified": False, "hardware_neural_inference_verified": False, "image_quality_verified": False, "real_fps_verified": False, "native_game_render_suppressed": False, "direct_gpu_presentation": False, "dwm_fallback": "If DWM cloak is unavailable only asynchronous hiding is available; immediate visual concealment is not guaranteed"}
    info = {"version": VERSION, "file_version": FILE_VERSION, "commit": args.commit, "source_commit": args.commit, "validated_code_commit": args.commit, "ci": ci, "actual_pe_verified": actual, "binaries": binaries, "source_archive": provenance, "default_neural_enabled": False, "nr_runtime_bundled": False, "limitations": limits}
    update = {name: (data, 0o100644) for name, data in files.items()}
    for name, source in (("README.txt", PACKAGE + "/README.txt"), ("LICENSE-GPL.txt", "LICENSE-GPL"), ("THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.md"), ("xBRZ-LICENSE.txt", "source/third_party/xbrz/License.txt"), ("LICENSE-Vulkan-Headers.md", "source/third_party/vulkan/LICENSE.md"), ("LICENSE-Apache-2.0.txt", "source/third_party/vulkan/LICENSES/Apache-2.0.txt"), ("LICENSE-MIT.txt", "source/third_party/vulkan/LICENSES/MIT.txt"), ("README-Vulkan-Headers.md", "source/third_party/vulkan/README.ra2toolbox.md"), ("Install-DLSS5Runtime.ps1", "source/neural_renderer/Install-DLSS5Runtime.ps1"), ("RUNTIME_SETUP.md", "source/neural_renderer/RUNTIME_SETUP.md")):
        require(source in sources, "Required package source missing: " + source); update[name] = sources[source]
    update["Build-info.json"] = (encoded(info), 0o100644)
    update["Source-provenance.json"] = (encoded(provenance), 0o100644)
    update["SHA256SUMS.txt"] = ("".join(sha(data) + "  " + name + "\n" for name, (data, _) in sorted(update.items())).encode("ascii"), 0o100644)
    payloads = {f"RA2-Toolbox-v{VERSION}-Update.zip": make_zip(update), f"RA2-Toolbox-v{VERSION}-Source.zip": source_zip}
    assets = [{"name": name, "bytes": len(data), "sha256": sha(data)} for name, data in payloads.items()]
    manifest = dict(info, prerelease=True, assets=assets, update_files={name: {"bytes": len(data), "sha256": sha(data)} for name, (data, _) in sorted(update.items())})
    payloads["release-manifest.json"] = encoded(manifest)
    payloads["SHA256SUMS.txt"] = "".join(sha(data) + "  " + name + "\n" for name, data in payloads.items()).encode("ascii")
    args.out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.out, prefix=".pre7-") as temporary:
        stage = Path(temporary)
        for name, data in payloads.items():
            (stage / name).write_bytes(data)
        for name in payloads:
            os.replace(stage / name, args.out / name)
    print(json.dumps({"version": VERSION, "commit": args.commit, "files": [{"name": name, "bytes": len(data), "sha256": sha(data)} for name, data in payloads.items()]}, ensure_ascii=False))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, UnicodeError, json.JSONDecodeError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        print("Release packaging failed: " + str(error), file=sys.stderr)
        sys.exit(1)
