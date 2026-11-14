#!/usr/bin/env python3
"""Package verified 1.0.51-pre.5 outputs and publish the requested prerelease."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, sys, zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "dist-v151pre5"
BIN = ROOT / "build-v151pre5"
BASE = ROOT / "baseline-v150"
VERSION = "1.0.51-pre.5"
NAMES = ("RA2Toolbox.exe", "GameRuntime.dll", "GameRuntimeHost32.exe", "WebView2Loader.dll")

def sha(data):
    return hashlib.sha256(data).hexdigest()

def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True, encoding="utf-8").strip()

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
    assert before.get("checkout_commit", before["commit"]) == run("git", "rev-parse", "HEAD")
    assert before["files"] == inputs(), "Tracked sources changed during build"
    sys.path.insert(0, str(ROOT / "source/tools"))
    from verify_windows_outputs import verify, read_release_version
    assert read_release_version() == ("1.0.51.5", VERSION)
    actual, errors = verify(BIN, host_arch="x64", webview2=True)
    assert not errors, errors
    checks = ROOT / "diagnostics-v151pre5/checks"
    for path in ("entry-hotpath/result.json", "identity/result.json", "runtime-scope/result.json"):
        assert json.loads((checks / path).read_text(encoding="utf-8"))["pass"], path
    assert json.loads((checks / "tech-current/result.json").read_text(encoding="utf-8"))["required_checks_passed"]
    assert json.loads((checks / "runtime-scope/result.json").read_text(encoding="utf-8"))["windows_test_process_executed"]
    retention = json.loads((checks / "retention/result.json").read_text(encoding="utf-8"))
    assert retention["pass"] and not retention["expected_failure"] and len(retention["results"]) == 8
    assert all(row["exit_code"] == 0 for row in retention["results"])
    assert "PASS production world frame core:" in (checks / "core.log").read_text(encoding="utf-8")
    assert "PASS Host transport isolation:" in (checks / "core.log").read_text(encoding="utf-8")
    assert "PASS dodge routing:" in (checks / "dodge-routing/run.log").read_text(encoding="utf-8")
    assert "PASS counted execution observations:" in (checks / "observations.log").read_text(encoding="utf-8")
    assert json.loads((checks / "reverse/result.json").read_text(encoding="utf-8"))["pass"]
    assert json.loads((checks / "value-input/receipt.json").read_text(encoding="utf-8"))["passed"]
    assert "PASS game-thread stalls:" in (checks / "stalls.log").read_text(encoding="utf-8")
    assert "PASS 63 production own-ground" in (checks / "own-ground.log").read_text(encoding="utf-8")
    assert "PASS special reveal:" in (checks / "visibility.log").read_text(encoding="utf-8")
    panels = json.loads((checks / "superweapon/result.json").read_text(encoding="utf-8"))
    assert len(panels["results"]) == 4 and all(row["passed"] for row in panels["results"])
    assert "PASS actual watch Renderer" in (checks / "watch/result.json").read_text(encoding="utf-8")
    baseline_contract = json.loads((ROOT / "packages/v1.0.51-pre.5/baseline-v150.json").read_text(encoding="utf-8"))
    baseline_manifest = BASE / "RA2Toolbox-1.0.50-release-manifest.json"
    assert record(baseline_manifest.read_bytes()) == baseline_contract["assets"][baseline_manifest.name]
    baseline = json.loads((BASE / "RA2Toolbox-1.0.50-release-manifest.json").read_text(encoding="utf-8"))
    notes = (ROOT / "packages/v1.0.51-pre.5/RELEASE_NOTES.md").read_bytes()
    paired = {name: (BIN / name).read_bytes() for name in NAMES}
    current = {}
    archives = {}
    for kind in ("Update", "Full"):
        old_name = "RA2Toolbox-1.0.50-x64-" + kind + ".zip"
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
        for meta_name in ("BUILD_SOURCE_MANIFEST.json", "BUILD_TARGET.json", "WINDOWS_BUILD_ARTIFACTS.json"):
            files[meta_name] = (BIN / meta_name).read_bytes()
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
    dep_name = "RA2Toolbox-1.0.50-x64-Dependencies.zip"
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
        "build.log": (ROOT / "diagnostics-v151pre5/build.log").read_bytes(),
        "static-pe.log": (ROOT / "diagnostics-v151pre5/static-pe.log").read_bytes()}
    if (BIN / "CONSOLE_COMPILE.json").is_file():
        evidence["CONSOLE_COMPILE.json"] = (BIN / "CONSOLE_COMPILE.json").read_bytes()
    for path in (ROOT / "diagnostics-v151pre5/checks").rglob("*"):
        if path.is_file() and path.suffix in (".json", ".log"):
            evidence["checks/" + path.relative_to(ROOT / "diagnostics-v151pre5/checks").as_posix()] = path.read_bytes()
    path = OUT / ("RA2Toolbox-" + VERSION + "-verification.zip")
    archives[path.name] = archive(path, evidence)
    manifest = {"version": VERSION, "file_version": "1.0.51.5", "tag": "v" + VERSION,
        "source_commit": before["commit"], "source_tree": before["source_tree"],
        "source_inputs_unchanged_after_build": None if "imported_artifact" in before else True,
        "packaging_inputs_unchanged": True, "source_inputs_checked": len(before["files"]),
        "imported_artifact": before.get("imported_artifact"),
        "build_method": "GitHub Actions windows-2022; Zig 0.13.0",
        "build_artifacts": {n: record(d) for n, d in paired.items()},
        "actual_pe_verified": actual, "archives": archives,
        "packages": {k: {"files": {n: record(d) for n, d in f.items()}} for k, f in current.items()},
        "validation_scope": {"build_and_static_pe_passed": True,
            "package_crc_and_members_passed": True, "regressions_rerun_before_release": True,
            "live_windows_ui_tested": False, "game_tested": False, "multiplayer_tested": False},
        "prerelease": True,
        "inherited_functional_verification": {"source_commit": "6203eeb03ed980079cd56fbe4cc9db75fcfe098b", "workflow_run": 43428658645, "record": "packages/v1.0.50-SamplingFix/verification.json", "game_tested": False},
        "dependency_contract_unchanged": True,
        "dependencies_reused_from": "v1.0.50 (verified byte-identical archive)"}
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
    tag = "v" + VERSION
    manifest = json.loads((OUT / ("RA2Toolbox-" + VERSION + "-release-manifest.json")).read_text(encoding="utf-8"))
    commit = manifest["source_commit"]
    assert commit == run("git", "rev-parse", "HEAD") and manifest["tag"] == tag and manifest["prerelease"]
    contract = json.loads((ROOT / "packages/v1.0.51-pre.5/baseline-v150.json").read_text(encoding="utf-8"))

    def find_release():
        for page in range(1, 20):
            rows = api("repos/" + repo + "/releases?per_page=100&page=" + str(page))
            found = next((r for r in rows if r["tag_name"] == tag), None)
            if found:
                return found
            if len(rows) < 100:
                return None
        raise RuntimeError("Release pagination exceeded limit")

    head = api("repos/" + repo + "/git/ref/heads/codex/common-entry-hotpath-20261114")["object"]["sha"]
    assert head == commit, "Superseded build must not publish"

    existing = find_release()
    if existing:
        assert existing["draft"], "Refusing to replace an already published release"
        assert existing["target_commitish"] == commit, "Draft points to a different source commit"
    else:
        subprocess.run(["gh", "release", "create", tag, "--repo", repo, "--target", commit,
            "--title", "RA2 Toolbox " + VERSION, "--notes-file", "packages/v1.0.51-pre.5/RELEASE_NOTES.md",
            "--prerelease", "--draft"], cwd=ROOT, check=True)
    paths = sorted(OUT.glob("RA2Toolbox-" + VERSION + "-*"))
    assert len(paths) == 6, [p.name for p in paths]
    for path in paths:
        subprocess.run(["gh", "release", "upload", tag, str(path), "--repo", repo, "--clobber"], cwd=ROOT, check=True)
    release = find_release()
    assert release is not None and release["draft"]
    expected = {p.name: record(p.read_bytes()) for p in paths}
    assets = {a["name"]: a for a in release["assets"]}
    assert set(assets) == set(expected)
    for name, meta in expected.items():
        assert assets[name]["size"] == meta["bytes"] and assets[name]["digest"] == "sha256:" + meta["sha256"], name

    # Only fast-forward the reviewed source; another writer's main is never
    # replaced to make publication succeed. A rerun may find it already here.
    main = api("repos/" + repo + "/git/ref/heads/main")["object"]["sha"]
    if main != commit:
        assert main == contract["base_main"], "Main advanced; integrate the concurrent source before publishing"
        api("repos/" + repo + "/git/refs/heads/main", "-X", "PATCH", "-f", "sha=" + commit, "-F", "force=false")
    release = api("repos/" + repo + "/releases/" + str(release["id"]), "-X", "PATCH",
        "-F", "draft=false", "-F", "prerelease=true", "-f", "make_latest=false")
    assert not release["draft"] and release["prerelease"] and release["tag_name"] == tag
    ref = api("repos/" + repo + "/git/ref/tags/" + tag)["object"]
    if ref["type"] == "tag":
        ref = api("repos/" + repo + "/git/tags/" + ref["sha"])["object"]
    assert ref["type"] == "commit" and ref["sha"] == commit
    latest = api("repos/" + repo + "/releases/latest")
    assert latest["tag_name"] != tag
    write_json(OUT / "publication.json", {"release": release["html_url"], "release_id": release["id"],
        "tag": tag, "prerelease": True, "source_commit": commit, "latest_stable": latest["tag_name"], "assets": expected})
    print("PUBLISHED " + release["html_url"])

if __name__ == "__main__":
    {"snapshot": snapshot, "package": package, "publish": publish}[sys.argv[1]]()
