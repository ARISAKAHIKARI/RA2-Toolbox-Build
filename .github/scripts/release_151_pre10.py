#!/usr/bin/env python3
"""Publish the verified asynchronous sampling build from the pinned CI artifact."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = "98cbd9fdac0a50c1f928a8fa3bda002ed0206fd2"
RUN = "43640706706"
VERSION = "1.0.51-pre.10"
TAG = "v" + VERSION
NAMES = ("RA2Toolbox.exe", "GameRuntime.dll", "GameRuntimeHost32.exe", "WebView2Loader.dll")
OUT = ROOT / "dist-v151pre10"
IMPORTED = ROOT / "imported-v151pre10"
BASE = ROOT / "baseline-v151pre9"


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True, encoding="utf-8").strip()


def record(data):
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def archive(path, files):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name, data in sorted(files.items()):
            z.writestr(name, data)
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None and set(z.namelist()) == set(files)
        for name, data in files.items():
            assert z.read(name) == data, name
    return record(path.read_bytes())


def api(path):
    return json.loads(run("gh", "api", "repos/" + os.environ["GITHUB_REPOSITORY"] + "/" + path))


def package():
    assert run("git", "rev-parse", "HEAD:source") == run("git", "rev-parse", PRODUCT + ":source")
    assert api("git/ref/heads/main")["object"]["sha"] == PRODUCT
    workflow = api("actions/runs/" + RUN)
    assert workflow["head_sha"] == PRODUCT and workflow["conclusion"] == "success"
    windows = json.loads((IMPORTED / "checks/windows/result.json").read_text())
    assert windows["passed"] and len(windows["runs"]) == 6 and all(x["passed"] for x in windows["runs"])
    assert "PASS: 3 current outputs" in (IMPORTED / "checks/pe.log").read_text()
    target = json.loads((IMPORTED / "async-build/BUILD_TARGET.json").read_text())
    artifacts = json.loads((IMPORTED / "async-build/WINDOWS_BUILD_ARTIFACTS.json").read_text())
    assert target["assist_protocol"] == 109 and target["runtime_protocol"] == 4
    assert target["host"] == "x64" and target["runtime"] == target["broker"] == "x86"
    assert artifacts["file_version"] == "1.0.51.10" and artifacts["verified"]
    paired = {name: (IMPORTED / "async-build" / name).read_bytes() for name in NAMES}
    notes = (ROOT / "packages/v1.0.51-pre.10/RELEASE_NOTES.md").read_bytes()
    baseline = json.loads((BASE / "RA2Toolbox-1.0.51-pre.9-release-manifest.json").read_text())
    assert baseline["version"] == "1.0.51-pre.9"
    OUT.mkdir(exist_ok=True)
    archives = {}
    package_files = {}
    for kind in ("Update", "Full"):
        old_name = "RA2Toolbox-1.0.51-pre.9-x64-" + kind + ".zip"
        path = BASE / old_name
        assert record(path.read_bytes()) == baseline["archives"][old_name]
        with zipfile.ZipFile(path) as z:
            assert z.testzip() is None
            files = {name: z.read(name) for name in z.namelist()}
        assert {name: record(files[name]) for name in files} == baseline["packages"][kind]["files"]
        files.update(paired)
        files["README.txt"] = notes
        for name in ("BUILD_SOURCE_MANIFEST.json", "BUILD_TARGET.json", "WINDOWS_BUILD_ARTIFACTS.json"):
            files[name] = (IMPORTED / "async-build" / name).read_bytes()
        output = "RA2Toolbox-" + VERSION + "-x64-" + kind + ".zip"
        archives[output] = archive(OUT / output, files)
        package_files[kind] = {name: record(data) for name, data in files.items()}
    assert all(package_files["Full"].get(name) == meta for name, meta in package_files["Update"].items())
    evidence = {"checks/pe.log": (IMPORTED / "checks/pe.log").read_bytes(),
                "checks/windows/result.json": (IMPORTED / "checks/windows/result.json").read_bytes(),
                "checks/portable/result.json": (IMPORTED / "portable/async/result.json").read_bytes(),
                "BUILD_TARGET.json": (IMPORTED / "async-build/BUILD_TARGET.json").read_bytes()}
    verification = "RA2Toolbox-" + VERSION + "-verification.zip"
    archives[verification] = archive(OUT / verification, evidence)
    manifest = {"version": VERSION, "file_version": "1.0.51.10", "tag": TAG,
                "source_commit": PRODUCT, "workflow_run": int(RUN),
                "build_artifacts": {n: record(data) for n, data in paired.items()},
                "baseline": {"tag": "v1.0.51-pre.9", "source_commit": baseline["source_commit"]},
                "archives": archives, "packages": {k: {"files": v} for k, v in package_files.items()},
                "validation_scope": {"windows_dll_runs": 6, "windows_paired_build": True,
                                     "real_game_tested": False, "multiplayer_tested": False}}
    manifest_name = "RA2Toolbox-" + VERSION + "-release-manifest.json"
    (OUT / manifest_name).write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    paths = sorted(OUT.glob("RA2Toolbox-" + VERSION + "-*"))
    (OUT / ("RA2Toolbox-" + VERSION + "-SHA256SUMS.txt")).write_text(
        "".join(record(path.read_bytes())["sha256"] + "  " + path.name + "\n" for path in paths))
    print("PACKAGED", PRODUCT, sorted(archives))


def publish():
    assert api("git/ref/heads/main")["object"]["sha"] == PRODUCT
    repo = os.environ["GITHUB_REPOSITORY"]
    releases = api("releases?per_page=100")
    existing = next((r for r in releases if r["tag_name"] == TAG), None)
    if existing:
        assert existing["draft"] and existing["target_commitish"] == PRODUCT
    else:
        subprocess.run(["gh", "release", "create", TAG, "--repo", repo, "--target", PRODUCT,
                        "--title", "RA2 Toolbox " + VERSION, "--notes-file",
                        "packages/v1.0.51-pre.10/RELEASE_NOTES.md", "--prerelease", "--draft"], check=True)
    files = sorted(OUT.glob("RA2Toolbox-" + VERSION + "-*"))
    assert len(files) == 5
    subprocess.run(["gh", "release", "upload", TAG, *map(str, files), "--repo", repo, "--clobber"], check=True)
    release = next(r for r in api("releases?per_page=100") if r["tag_name"] == TAG)
    assert release["draft"] and len(release["assets"]) == len(files)
    expected = {p.name: record(p.read_bytes()) for p in files}
    for asset in release["assets"]:
        meta = expected[asset["name"]]
        assert asset["size"] == meta["bytes"] and asset["digest"] == "sha256:" + meta["sha256"]
    subprocess.run(["gh", "release", "edit", TAG, "--repo", repo, "--draft=false", "--prerelease", "--latest=false"], check=True)
    release = next(r for r in api("releases?per_page=100") if r["tag_name"] == TAG)
    assert not release["draft"] and release["prerelease"]
    print("PUBLISHED", release["html_url"], PRODUCT, sorted(expected))


if __name__ == "__main__":
    {"package": package, "publish": publish}[sys.argv[1]]()
