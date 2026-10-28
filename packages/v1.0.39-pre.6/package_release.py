#!/usr/bin/env python3
"""Package the exact verified xBRZ/Air120 build without historical release gates."""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
VERSION = "1.0.39-pre.6"
FILE_VERSION = "1.0.39.6"
BUILD_SHA = "b372ace50101e0099a5e9533c50470b4cabb1536"
BUILD_RUN = "40687783887"
PACKAGE_DIR = f"packages/v{VERSION}"
BINARY_NAMES = ("RA2Toolbox.exe", "GameRuntime.dll")
DIAGNOSTIC_NAMES = ("render-build.json", "render-tests.log", "build-render-upgrade.log")
ROOT_SOURCES = {"AGENTS.md", "README.md", "LICENSE-GPL", "THIRD_PARTY_NOTICES.md", ".gitattributes"}
WORKFLOWS = {".github/workflows/validate-render-upgrade.yml", ".github/workflows/publish-v139-pre6.yml"}
FORBIDDEN_SUFFIXES = {".exe", ".dll", ".pdb", ".zip", ".o", ".obj", ".lib", ".a", ".so", ".dylib", ".pyc", ".pyo", ".whl", ".7z", ".rar", ".dmp", ".dump", ".sqlite", ".db"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def committed_files(commit):
    """Read committed blobs, never ignored files, working files or symlink targets."""
    require(git("cat-file", "-t", commit).strip() == b"commit", "GITHUB_SHA must identify a commit")
    files = {}
    records = git("ls-tree", "-r", "-z", "--full-tree", commit).split(b"\0")
    for record in records:
        if not record:
            continue
        info, raw_path = record.split(b"\t", 1)
        mode, kind, blob = info.decode("ascii").split()
        path = raw_path.decode("utf-8")
        selected = path in ROOT_SOURCES or path in WORKFLOWS or path.startswith(("source/", "docs/", PACKAGE_DIR + "/"))
        if not selected:
            continue
        parts = PurePosixPath(path).parts
        require(not any(p in {".", "..", ".git"} for p in parts) and "\\" not in path and "\n" not in path and "\r" not in path,
                f"Unsafe archive path: {path!r}")
        require(mode in {"100644", "100755"} and kind == "blob", f"Non-regular source entry is forbidden: {path}")
        if PurePosixPath(path).suffix.lower() in FORBIDDEN_SUFFIXES or "__pycache__" in parts:
            continue
        files[path] = (git("cat-file", "blob", blob), int(mode, 8))
    required = ROOT_SOURCES | WORKFLOWS | {
        PACKAGE_DIR + "/RELEASE_NOTES.md", PACKAGE_DIR + "/README.txt", PACKAGE_DIR + "/package_release.py",
        "source/shared/source/release_version.h", "source/third_party/xbrz/License.txt",
        "source/third_party/xbrz/UPSTREAM.json", "source/third_party/xbrz/xbrz.cpp",
    }
    require(required <= files.keys(), "Source commit is missing required release files: " + ", ".join(sorted(required - files.keys())))
    require(files[PACKAGE_DIR + "/package_release.py"][0] == Path(__file__).read_bytes(), "Running packager differs from committed source")
    header = files["source/shared/source/release_version.h"][0].decode("utf-8")
    require(f'#define RA2_RELEASE_VERSION "{FILE_VERSION}"' in header and f'L"{VERSION}"' in header,
            "Committed release version does not match this package")
    upstream = json.loads(files["source/third_party/xbrz/UPSTREAM.json"][0].decode("utf-8"))
    require(upstream.get("version") == "1.9" and upstream.get("local_modifications") is False, "Unexpected xBRZ provenance")
    for entry in upstream["files"]:
        path = "source/third_party/xbrz/" + entry["path"]
        require(path in files and digest(files[path][0]) == entry["sha256"], f"Upstream xBRZ source mismatch: {path}")
    return files


def zip_bytes(path, files, timestamp):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, (data, mode) in sorted(files.items()):
            require(not name.startswith("/") and ".." not in PurePosixPath(name).parts, f"Unsafe ZIP member: {name}")
            item = zipfile.ZipInfo(name, timestamp)
            item.create_system = 3
            item.compress_type = zipfile.ZIP_DEFLATED
            item.external_attr = mode << 16
            archive.writestr(item, data)
    with zipfile.ZipFile(path) as archive:
        require(archive.testzip() is None, "ZIP CRC failure: " + path.name)
        require(set(archive.namelist()) == set(files), "Unexpected ZIP contents: " + path.name)
        for name, (data, _) in files.items():
            require(digest(archive.read(name)) == digest(data), "ZIP payload mismatch: " + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binaries", required=True, type=Path)
    parser.add_argument("--diagnostics", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    require(os.environ.get("BUILD_SHA") == BUILD_SHA, "BUILD_SHA does not match the verified build")
    require(os.environ.get("BUILD_RUN") == BUILD_RUN, "BUILD_RUN does not match the verified run")
    source_commit = os.environ.get("GITHUB_SHA", "")
    require(re.fullmatch(r"[0-9a-f]{40}", source_commit) is not None, "A full GITHUB_SHA is required")
    sources = committed_files(source_commit)
    diagnostics = {name: (args.diagnostics / name).read_bytes() for name in DIAGNOSTIC_NAMES}
    report = json.loads(diagnostics["render-build.json"].decode("utf-8-sig"))
    require(report.get("commit") == BUILD_SHA and str(report.get("run")) == BUILD_RUN, "CI build identity mismatch")
    require(report.get("xbrz_integrated") is True and report.get("assist_protocol") == 80, "Expected xBRZ/IPC80 CI build")
    require(report.get("live_game_verified") is False and report.get("multiplayer_verified") is False,
            "CI validation limits changed; review release notes")
    declared = report.get("files", [])
    require(isinstance(declared, list) and len(declared) == 2 and {x.get("name") for x in declared} == set(BINARY_NAMES),
            "CI build must declare exactly the EXE/DLL pair")
    pair = {name: (args.binaries / name).read_bytes() for name in BINARY_NAMES}
    binaries = {}
    for record in declared:
        name = record["name"]
        require(type(record.get("bytes")) is int and record["bytes"] == len(pair[name]) and len(pair[name]) > 0,
                "CI binary size mismatch: " + name)
        require(isinstance(record.get("sha256"), str) and re.fullmatch(r"[0-9a-f]{64}", record["sha256"]) is not None
                and digest(pair[name]) == record["sha256"], "CI binary hash mismatch: " + name)
        require((ROOT / "source" / name).read_bytes() == pair[name], "Verifier input differs from release binary: " + name)
        binaries[name] = {"bytes": len(pair[name]), "sha256": digest(pair[name])}
    fresh_metadata_path = ROOT / "source/WINDOWS_BUILD_ARTIFACTS.json"
    fresh_metadata_bytes = fresh_metadata_path.read_bytes()
    metadata = json.loads(fresh_metadata_bytes.decode("utf-8-sig"))
    require(metadata.get("file_version") == FILE_VERSION, "Run verify_windows_outputs.py against the CI binaries first")
    for name, record in binaries.items():
        verified = metadata.get("verified", {}).get(name, {})
        require(verified.get("sha256") == record["sha256"] and verified.get("bytes") == record["bytes"]
                and verified.get("assist_protocol") == 80, "Fresh PE/ABI verification mismatch: " + name)
    limits = {
        "prerelease": True, "live_game_verified": False, "multiplayer_verified": False,
        "real_fps_verified": False, "real_latency_verified": False, "hdr_output": False,
        "gpu_super_resolution": False, "all_native_effects_complete": False,
        "fractional_scaling_cpu_cost": "May be high; synthetic tests do not establish real-game FPS",
        "aircraft_damage": "Estimated supported weapon volley; no guarantee of hits or kills",
    }
    verification = {
        "version": VERSION, "file_version": FILE_VERSION, "assist_protocol": 80,
        "validated_code_commit": BUILD_SHA, "source_commit": source_commit, "validation_run": BUILD_RUN,
        "ci_url": f"https://github.com/ARISAKAHIKARI/RA2-Toolbox-Build/actions/runs/{BUILD_RUN}",
        "ci_build": report, "pe_abi_verification": metadata, "binaries": binaries,
        "diagnostic_sha256": {name: digest(data) for name, data in diagnostics.items()}, "limitations": limits,
    }
    update_files = {name: (data, 0o100644) for name, data in pair.items()}
    update_files.update({
        "README.txt": sources[PACKAGE_DIR + "/README.txt"],
        "LICENSE-GPL.txt": sources["LICENSE-GPL"],
        "xBRZ-LICENSE.txt": sources["source/third_party/xbrz/License.txt"],
        "THIRD_PARTY_NOTICES.md": sources["THIRD_PARTY_NOTICES.md"],
        "verification.json": ((json.dumps(verification, ensure_ascii=False, indent=2) + "\n").encode("utf-8"), 0o100644),
    })
    source_files = dict(sources)
    for name, data in diagnostics.items():
        source_files["release-verification/" + name] = (data, 0o100644)
    source_files["release-verification/WINDOWS_BUILD_ARTIFACTS.json"] = (fresh_metadata_bytes, 0o100644)
    source_files["release-verification/verification.json"] = update_files["verification.json"]
    epoch = int(git("show", "-s", "--format=%ct", source_commit).strip())
    stamp = dt.datetime.fromtimestamp(epoch, dt.timezone.utc)
    require(1980 <= stamp.year <= 2107, "Commit timestamp is outside ZIP range")
    timestamp = (stamp.year, stamp.month, stamp.day, stamp.hour, stamp.minute, stamp.second)
    args.out.mkdir(parents=True, exist_ok=True)
    assets = []
    for kind, payload in (("Update", update_files), ("Source", source_files)):
        path = args.out / f"RA2-Toolbox-v{VERSION}-{kind}.zip"
        zip_bytes(path, payload, timestamp)
        assets.append({"name": path.name, "bytes": path.stat().st_size, "sha256": digest(path.read_bytes()),
                       "files": {name: {"bytes": len(data), "sha256": digest(data)} for name, (data, _) in sorted(payload.items())}})
    manifest = {
        "version": VERSION, "file_version": FILE_VERSION, "assist_protocol": 80, "prerelease": True,
        "validated_code_commit": BUILD_SHA, "source_commit": source_commit, "validation_run": BUILD_RUN,
        "ci_url": verification["ci_url"], "assets": assets, "binaries": binaries, "limitations": limits,
    }
    manifest_path = args.out / "release-manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    sums = [f"{a['sha256']}  {a['name']}\n" for a in assets]
    sums.append(f"{digest(manifest_path.read_bytes())}  {manifest_path.name}\n")
    (args.out / "SHA256SUMS.txt").write_text("".join(sums), encoding="ascii")
    print(json.dumps({"version": VERSION, "source_commit": source_commit,
                      "assets": [{k: v for k, v in a.items() if k != "files"} for a in assets]}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, UnicodeError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print(f"Release packaging failed: {error}", file=sys.stderr)
        sys.exit(1)
