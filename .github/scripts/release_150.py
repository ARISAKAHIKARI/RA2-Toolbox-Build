#!/usr/bin/env python3
"""Package verified v1.0.50 outputs and publish only the requested release."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, sys, zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "dist-v150"
BIN = ROOT / "build-v150"
BASE = ROOT / "baseline-v149"
VERSION = "1.0.50"
NAMES = ("RA2Toolbox.exe", "GameRuntime.dll", "GameRuntimeHost32.exe", "WebView2Loader.dll")

def sha(data):
    return hashlib.sha256(data).hexdigest()

def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()

def record(data):
    return {"bytes": len(data), "sha256": sha(data)}

def inputs():
    paths = run("git", "ls-files", "source").splitlines()
    return {p: record((ROOT / p).read_bytes()) for p in paths}

def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def archive(path, files):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name, data in sorted(files.items()):
            z.writestr(name, data)
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        assert set(z.namelist()) == set(files)
        for name, data in files.items():
            assert z.read(name) == data, name
    return record(path.read_bytes())

def snapshot():
    OUT.mkdir(exist_ok=True)
    assert not BIN.exists() or not any(BIN.iterdir()), "Build directory must be empty"
    write_json(OUT / "source-inputs.json", {"commit": run("git", "rev-parse", "HEAD"),
        "source_tree": run("git", "rev-parse", "HEAD:source"), "files": inputs()})

def package():
    before = json.loads((OUT / "source-inputs.json").read_text(encoding="utf-8"))
    assert before["commit"] == run("git", "rev-parse", "HEAD")
    assert before["files"] == inputs(), "Tracked sources changed during build"
    sys.path.insert(0, str(ROOT / "source/tools"))
    from verify_windows_outputs import verify, read_release_version
    assert read_release_version() == ("1.0.50.0", VERSION)
    actual, errors = verify(BIN, host_arch="x64", webview2=True)
    assert not errors, errors
    baseline = json.loads((ROOT / "packages/v1.0.49/release-manifest.json").read_text(encoding="utf-8"))
    notes = (ROOT / "packages/v1.0.50/RELEASE_NOTES.md").read_bytes()
    paired = {name: (BIN / name).read_bytes() for name in NAMES}
    current = {}
    archives = {}
    for kind in ("Update", "Full"):
        old_name = "RA2Toolbox-1.0.49-x64-" + kind + ".zip"
        old_path = BASE / old_name
        assert record(old_path.read_bytes()) == baseline["archives"][old_name], old_name
        with zipfile.ZipFile(old_path) as z:
            assert z.testzip() is None
            expected = baseline["packages"][kind]["files"]
            files = {}
            for name, meta in expected.items():
                data = z.read(name)
                assert record(data) == meta, name
                files[name] = data
        files.update(paired)
        files["README.txt"] = notes
        files["licenses/LICENSE-GPL.txt"] = (ROOT / "LICENSE-GPL").read_bytes()
        files["licenses/THIRD_PARTY_NOTICES.md"] = (ROOT / "THIRD_PARTY_NOTICES.md").read_bytes()
        files["licenses/xbrz-License.txt"] = (ROOT / "source/third_party/xbrz/License.txt").read_bytes()
        for name in ("LICENSE.txt", "NOTICE.txt"):
            files["licenses/WebView2/" + name] = (BIN / "licenses/WebView2" / name).read_bytes()
        for name in list(files):
            if name.startswith("engine/"):
                files[name] = (ROOT / "source" / name).read_bytes()
        current[kind] = files
        path = OUT / ("RA2Toolbox-" + VERSION + "-x64-" + kind + ".zip")
        archives[path.name] = archive(path, files)
    assert all(current["Full"][n] == d for n, d in current["Update"].items())
    dep_name = "RA2Toolbox-1.0.49-x64-Dependencies.zip"
    dep_data = (BASE / dep_name).read_bytes()
    assert record(dep_data) == baseline["archives"][dep_name]
    with zipfile.ZipFile(BASE / dep_name) as z:
        assert z.testzip() is None
    dependency = OUT / ("RA2Toolbox-" + VERSION + "-x64-Dependencies.zip")
    dependency.write_bytes(dep_data)
    archives[dependency.name] = record(dep_data)
    evidence = {"source-inputs.json": (OUT / "source-inputs.json").read_bytes(),
        "WINDOWS_BUILD_ARTIFACTS.json": (BIN / "WINDOWS_BUILD_ARTIFACTS.json").read_bytes(),
        "BUILD_TARGET.json": (BIN / "BUILD_TARGET.json").read_bytes(),
        "build.log": (ROOT / "diagnostics-v150/build.log").read_bytes(),
        "static-pe.log": (ROOT / "diagnostics-v150/static-pe.log").read_bytes()}
    path = OUT / ("RA2Toolbox-" + VERSION + "-verification.zip")
    archives[path.name] = archive(path, evidence)
    manifest = {"version": VERSION, "file_version": "1.0.50.0", "tag": "v1.0.50",
        "source_commit": before["commit"], "source_tree": before["source_tree"],
        "source_inputs_unchanged_after_build": True, "source_inputs_checked": len(before["files"]),
        "build_method": "GitHub Actions windows-2022; Zig 0.13.0",
        "build_artifacts": {n: record(d) for n, d in paired.items()},
        "actual_pe_verified": actual, "archives": archives,
        "packages": {k: {"files": {n: record(d) for n, d in f.items()}} for k, f in current.items()},
        "validation_scope": {"build_and_static_pe_passed": True,
            "package_crc_and_members_passed": True, "regressions_rerun_before_release": False,
            "live_windows_ui_tested": False, "game_tested": False, "multiplayer_tested": False},
        "dependency_contract_unchanged": True,
        "dependencies_reused_from": "v1.0.49 (verified byte-identical archive)"}
    manifest_path = OUT / ("RA2Toolbox-" + VERSION + "-release-manifest.json")
    write_json(manifest_path, manifest)
    paths = [OUT / n for n in archives] + [manifest_path]
    (OUT / ("RA2Toolbox-" + VERSION + "-SHA256SUMS.txt")).write_text(
        "".join(sha(p.read_bytes()) + "  " + p.name + "\n" for p in paths), encoding="utf-8")
    print(json.dumps({"version": VERSION, "source_commit": before["commit"], "archives": archives}, indent=2))

def api(path, *options):
    return json.loads(run("gh", "api", path, *options))

def publish():
    repo = os.environ["GITHUB_REPOSITORY"]
    commit = run("git", "rev-parse", "HEAD")
    tag = "v1.0.50"
    subprocess.run(["git", "push", "origin", "HEAD:refs/heads/main"], cwd=ROOT, check=True)
    def find_release():
        for page in range(1, 20):
            rows = api("repos/" + repo + "/releases?per_page=100&page=" + str(page))
            match = next((r for r in rows if r["tag_name"] == tag), None)
            if match:
                return match
            if len(rows) < 100:
                return None
        raise RuntimeError("Release pagination exceeded limit")
    existing = find_release()
    if existing:
        assert existing["draft"], "Refusing to replace an already published release"
        assert existing["target_commitish"] == commit, "Draft points to a different commit"
    else:
        subprocess.run(["gh", "release", "create", tag, "--repo", repo, "--target", commit,
            "--title", "RA2 Toolbox 1.0.50", "--notes-file", "packages/v1.0.50/RELEASE_NOTES.md",
            "--draft"], cwd=ROOT, check=True)
    paths = sorted(OUT.glob("RA2Toolbox-1.0.50-*"))
    assert len(paths) == 6, [str(p) for p in paths]
    for p in paths:
        subprocess.run(["gh", "release", "upload", tag, str(p), "--repo", repo, "--clobber"],
            cwd=ROOT, check=True)
    release = find_release()
    assert release is not None, "Draft release missing"
    expected = {p.name: record(p.read_bytes()) for p in paths}
    assets = {a["name"]: a for a in release["assets"]}
    assert set(assets) == set(expected)
    for name, meta in expected.items():
        assert assets[name]["size"] == meta["bytes"], name
        assert assets[name]["digest"] == "sha256:" + meta["sha256"], name
    release = api("repos/" + repo + "/releases/" + str(release["id"]), "-X", "PATCH",
        "-F", "draft=false", "-F", "prerelease=false", "-f", "make_latest=true")
    assert not release["draft"] and release["tag_name"] == tag
    old = api("repos/" + repo + "/releases/tags/v1.0.49")
    assert old["tag_name"] == "v1.0.49"
    old = api("repos/" + repo + "/releases/" + str(old["id"]), "-X", "PATCH", "-F", "draft=true")
    assert old["draft"]
    latest = api("repos/" + repo + "/releases/latest")
    assert latest["tag_name"] == tag
    write_json(OUT / "publication.json", {"release": release["html_url"], "tag": tag,
        "source_commit": commit, "old_release_id": old["id"], "old_release_draft": old["draft"],
        "assets": expected})
    print("PUBLISHED " + release["html_url"] + "; v1.0.49 retracted as draft")

if __name__ == "__main__":
    {"snapshot": snapshot, "package": package, "publish": publish}[sys.argv[1]]()
