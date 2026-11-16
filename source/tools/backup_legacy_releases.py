#!/usr/bin/env python3
"""Back up legacy GitHub Releases before any destructive cleanup.

Requires gh CLI and GH_TOKEN (GitHub Actions contents:write/read). The backup
contains full original release JSON plus every attached asset, deduplicated
by SHA-256. The GitHub Actions job must upload *all* backup objects
successfully before invoking the cleanup subcommand.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def gh(*args, stdout=None):
    cmd = ["gh", *map(str, args)]
    if stdout is None:
        return subprocess.check_output(cmd, text=True)
    subprocess.run(cmd, stdout=stdout, check=True)
    return None


def api(repo, path):
    return json.loads(gh("api", f"/repos/{repo}/{path}"))


def releases(repo):
    results = []
    for page in range(1, 21):
        batch = api(repo, f"releases?per_page=100&page={page}")
        if not isinstance(batch, list):
            raise RuntimeError("Unexpected GitHub releases API response")
        results.extend(batch)
        if len(batch) < 100:
            break
    else:
        raise RuntimeError("Release list exceeded bounded pagination")
    ids = [x["id"] for x in results]
    if len(ids) != len(set(ids)):
        raise RuntimeError("Repeated release IDs in GitHub API")
    return results


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for part in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            h.update(part)
    return h.hexdigest()


def backup(repo, root):
    target = "v1.0.51"
    root.mkdir(parents=True, exist_ok=True)
    objects = root / "objects"
    objects.mkdir(exist_ok=True)
    metadata = root / "releases"
    metadata.mkdir(exist_ok=True)
    all_releases = releases(repo)
    candidates = [r for r in all_releases if r["draft"] or r["prerelease"]]
    if any(r["tag_name"] == target for r in candidates):
        raise RuntimeError("Refusing to archive prospective latest stable release")
    if not candidates:
        raise RuntimeError("Nothing to archive: unexpected release inventory")
    manifest = {
        "repo": repo,
        "latest_target": target,
        "backup_format": 1,
        "release_count": len(candidates),
        "releases": [],
    }
    unique_bytes = 0
    source_bytes = 0
    for index, item in enumerate(candidates, 1):
        rid = item["id"]
        tag = item["tag_name"]
        assets = item["assets"]
        if not assets:
            raise RuntimeError(f"Refusing to archive empty release {tag}")
        (metadata / f"{rid}.json").write_text(
            json.dumps(item, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
        with tempfile.TemporaryDirectory(prefix="ra2-legacy-release-") as scratch:
            folder = Path(scratch)
            # Authenticated gh release download can fetch draft assets.
            subprocess.run(
                ["gh", "release", "download", tag, "--repo", repo, "--dir", str(folder)],
                check=True,
            )
            expected = set()
            record = {
                "id": rid,
                "tag": tag,
                "draft": item["draft"],
                "prerelease": item["prerelease"],
                "assets": [],
            }
            for asset in assets:
                name = asset["name"]
                if Path(name).name != name or not name or name in expected:
                    raise RuntimeError(f"Unexpected asset name: {name!r}")
                expected.add(name)
                src = folder / name
                if not src.is_file() or src.stat().st_size != asset["size"]:
                    raise RuntimeError(f"Missing/wrong-sized {tag}/{name}")
                digest = sha256(src)
                api_digest = asset.get("digest") or ""
                if api_digest.startswith("sha256:") and api_digest[7:].lower() != digest:
                    raise RuntimeError(f"SHA-256 does not match GitHub digest for {tag}/{name}")
                destination = objects / digest[:2] / digest
                destination.parent.mkdir(exist_ok=True)
                source_bytes += asset["size"]
                if not destination.exists():
                    shutil.move(src, destination)
                    unique_bytes += asset["size"]
                else:
                    if sha256(destination) != digest or destination.stat().st_size != asset["size"]:
                        raise RuntimeError(f"Corrupt deduplicated asset {digest}")
                    src.unlink()
                record["assets"].append({
                    "name": name,
                    "asset_id": asset["id"],
                    "size": asset["size"],
                    "sha256": digest,
                })
            extras = {p.name for p in folder.iterdir() if p.is_file()}
            if extras:
                raise RuntimeError(f"Unaccounted assets in {tag}: {sorted(extras)}")
            manifest["releases"].append(record)
        print(f"[{index}/{len(candidates)}] archived {tag}: {len(assets)} assets", flush=True)
    manifest["original_asset_bytes"] = source_bytes
    manifest["unique_asset_bytes"] = unique_bytes
    (root / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    assert all((metadata / f'{r["id"]}.json').exists() for r in manifest["releases"])
    assert len({r["id"] for r in manifest["releases"]}) == len(candidates)
    print(
        f"BACKUP VERIFIED: {len(candidates)} legacy releases, "
        f"{source_bytes} source bytes, {unique_bytes} deduplicated bytes",
        flush=True,
    )


def cleanup(repo, manifest_path):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest["repo"] != repo or manifest["latest_target"] != "v1.0.51":
        raise RuntimeError("Backup manifest does not match target")
    backed = {r["id"] for r in manifest["releases"]}
    if len(backed) != manifest["release_count"] or not backed:
        raise RuntimeError("Incomplete release backup manifest")
    latest = api(repo, "releases/latest")
    if latest["tag_name"] != "v1.0.51" or latest["draft"] or latest["prerelease"]:
        raise RuntimeError("v1.0.51 is not GitHub's public stable Latest")
    live = releases(repo)
    original_stable = {r["tag_name"] for r in live if not r["draft"] and not r["prerelease"]}
    if not {"v1.0.51", "v1.0.50", "v1.0.48.1"}.issubset(original_stable):
        raise RuntimeError("Stable releases missing; refusing cleanup")
    pending = [r for r in live if r["draft"] or r["prerelease"]]
    unexpected = [r for r in pending if r["id"] not in backed]
    if unexpected:
        raise RuntimeError("Refusing to delete unbacked releases: "
                           + ", ".join(r["tag_name"] for r in unexpected))
    for r in pending:
        print(f"DELETE release record {r['id']} {r['tag_name']}", flush=True)
        gh("api", "--method", "DELETE", f"/repos/{repo}/releases/{r['id']}")
    remaining = releases(repo)
    bad = [r["tag_name"] for r in remaining if r["draft"] or r["prerelease"]]
    if bad:
        raise RuntimeError(f"Still contains pre-releases/drafts: {bad}")
    tags = {r["tag_name"] for r in remaining}
    if not {"v1.0.51", "v1.0.50", "v1.0.48.1"}.issubset(tags):
        raise RuntimeError("Stable Release record disappeared during cleanup")
    if api(repo, "releases/latest")["tag_name"] != "v1.0.51":
        raise RuntimeError("GitHub's Latest no longer points to v1.0.51")
    print(
        f"CLEANUP VERIFIED: deleted {len(pending)} release records; "
        f"{len(remaining)} stable releases retained; "
        "tags were intentionally not deleted.",
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("operation", choices=["backup", "cleanup"])
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY"))
    parser.add_argument("--root", type=Path, default=Path("legacy-release-backup"))
    args = parser.parse_args()
    if not args.repo:
        parser.error("--repo or GITHUB_REPOSITORY is required")
    if args.operation == "backup":
        backup(args.repo, args.root)
    else:
        cleanup(args.repo, args.root / "manifest.json")
