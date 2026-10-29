#!/usr/bin/env python3
"""Prepare private x64 NGX build dependencies from pinned NVIDIA publications.

This downloads the proprietary SDK for local development. It does not execute
NVIDIA DLLs, modify a game, install a driver, or redistribute a standalone SDK.
Keep the downloaded SDK outside the source tree and include NVIDIA's terms in
any distribution permitted by those terms.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
import urllib.request

BASE_COMMIT = "374959484e79a640feaba44c93ac8cfb0a03f5b5"
BASE_ORIGIN = "https://raw.githubusercontent.com/NVIDIA/DLSS/" + BASE_COMMIT + "/"
NR_ARCHIVE_URL = "https://d1oqksizpb0e3d.cloudfront.net/rtx-remix-ngx_sdk_dlnr%402.7z"
NR_ARCHIVE_BYTES = 367851723
NR_ARCHIVE_SHA256 = "4bb8651b3ac2180f23df1f39422c489810a13d445a94dc836c876c18c93ecefe"
RUNTIME_PATH = "lib/Windows_x86_64/rel/nvngx_dlssnr.dll"
# The base Git blobs are pinned to NVIDIA/DLSS. The NR overlay is the public
# rtx-remix-ngx_sdk_dlnr version2 dependency in NVIDIA's pinned manifest:
# NVIDIAGameWorks/dxvk-remix/0ba9973fd856e29fe9cf8dd2f2bc754bcbb2037a.

BASE_FILES = {
  "LICENSE.txt": {
    "bytes": 26620,
    "git_blob": "80756566d773cdac55179c5ae16c4495df7f73b5"
  },
  "include/nvsdk_ngx.h": {
    "bytes": 40289,
    "git_blob": "ae6b49174049eed69cc377b2fffd3c901651f764"
  },
  "include/nvsdk_ngx_defs.h": {
    "bytes": 44313,
    "git_blob": "807fa2475e90214f6846b9b4840be92e7bd66b5b"
  },
  "include/nvsdk_ngx_defs_dlssd.h": {
    "bytes": 14693,
    "git_blob": "8b73e3f347fab4f315443869ad57eeeb61168d7f"
  },
  "include/nvsdk_ngx_defs_dlssg.h": {
    "bytes": 22885,
    "git_blob": "3337ad5e8b2221bc914069b4171c2bc6461e2eed"
  },
  "include/nvsdk_ngx_defs_vk.h": {
    "bytes": 3196,
    "git_blob": "f02503e1ce90e5320053ca10883883ae71100136"
  },
  "include/nvsdk_ngx_helpers.h": {
    "bytes": 5317,
    "git_blob": "e6a0f2581e1586240821afa7523a65a75654e847"
  },
  "include/nvsdk_ngx_helpers_cuda.h": {
    "bytes": 3593,
    "git_blob": "251ba9d5fcf0800bbc3fbb5e8870c61739c6d98a"
  },
  "include/nvsdk_ngx_helpers_d3d.h": {
    "bytes": 31976,
    "git_blob": "664f99d95d3f8375304dc558ec601a66635ade7b"
  },
  "include/nvsdk_ngx_helpers_dlssd.h": {
    "bytes": 732,
    "git_blob": "479c949ade46e1fcbef05f5e49ba8f5e6b359a03"
  },
  "include/nvsdk_ngx_helpers_dlssd_cuda.h": {
    "bytes": 29175,
    "git_blob": "cebba04f16e581bafe3d94d46b865f9b55e37147"
  },
  "include/nvsdk_ngx_helpers_dlssd_d3d.h": {
    "bytes": 72668,
    "git_blob": "4e9c30f036be43bfe51c91e8a5d6a24bc218bfeb"
  },
  "include/nvsdk_ngx_helpers_dlssd_vk.h": {
    "bytes": 38337,
    "git_blob": "2fda1bbb62ba1b5b60010cf3949cef6fad61d2a2"
  },
  "include/nvsdk_ngx_helpers_dlssg.h": {
    "bytes": 732,
    "git_blob": "45d7815ae1c0b79aa2fff88d543e6c5e22ebb8cf"
  },
  "include/nvsdk_ngx_helpers_dlssg_d3d.h": {
    "bytes": 17445,
    "git_blob": "351245357a197aefb8c16189703ccc7656ded261"
  },
  "include/nvsdk_ngx_helpers_dlssg_vk.h": {
    "bytes": 17077,
    "git_blob": "9db4ff7de4209495adceb56ab1c759d868200418"
  },
  "include/nvsdk_ngx_helpers_vk.h": {
    "bytes": 19720,
    "git_blob": "664ac5302ea62cb57cd03a3779cbe2fc0a418397"
  },
  "include/nvsdk_ngx_loader.h": {
    "bytes": 13252,
    "git_blob": "13756a449673327b08dfc9e6715937636de4ca48"
  },
  "include/nvsdk_ngx_params.h": {
    "bytes": 7744,
    "git_blob": "014d9f4adec600ea3677947b7f98258da9f9c7fc"
  },
  "include/nvsdk_ngx_params_dlssd.h": {
    "bytes": 1199,
    "git_blob": "6950ed305ba4d2eba1a2eab3c6463fec9ce10e6d"
  },
  "include/nvsdk_ngx_params_dlssg.h": {
    "bytes": 6137,
    "git_blob": "69c6715e28456380a92e658a482d243f6c96b43e"
  },
  "include/nvsdk_ngx_standalone_common.h": {
    "bytes": 12985,
    "git_blob": "e765305ff16f92714269410a76c2bac3ffdb76f8"
  },
  "include/nvsdk_ngx_standalone_cuda.h": {
    "bytes": 10906,
    "git_blob": "98c248bbac9ec263564ae5fe7735db4a6afac1c5"
  },
  "include/nvsdk_ngx_vk.h": {
    "bytes": 38382,
    "git_blob": "70d895409132b68eff0d6027fb1ea2552d8c749b"
  },
  "lib/Windows_x86_64/vs2010/x64/nvsdk_ngx_s.lib": {
    "bytes": 3573218,
    "git_blob": "e04b74168373964478227837377e87de9c3ba60c"
  },
  "lib/Windows_x86_64/x64/nvsdk_ngx_s.lib": {
    "bytes": 3782558,
    "git_blob": "fcd9adc26d111e7013be555d27a4cf60e5753eb5"
  }
}

NR_FILES = {
  "LICENSE.txt": {
    "bytes": 26727,
    "sha256": "3027f23ca5a46dd9cb8183fbd522983a86f64d7daac5982912bf9f214671f294"
  },
  "include/nvsdk_ngx.h": {
    "bytes": 41001,
    "sha256": "96f21266a05ef06bb49f58c89de1a7845f2d9b1e0e0e069fe03740b8eaf0344d"
  },
  "include/nvsdk_ngx_defs_dlssnr.h": {
    "bytes": 11465,
    "sha256": "16b0ff55b136e6dd65ab8ef23b530e91f2f4fcd11f36d75e1175b9d99d7e0c5c"
  },
  "include/nvsdk_ngx_defs_vk.h": {
    "bytes": 3302,
    "sha256": "43b821c5d297d86165e6d2f96a476f5c30f2158c7d5a0451425c6b927abfec88"
  },
  "include/nvsdk_ngx_helpers_dlssnr.h": {
    "bytes": 4399,
    "sha256": "d173bc9436406e144c8f2b955424569ff7215207acb08240401928175e1207a1"
  },
  "include/nvsdk_ngx_helpers_dlssnr_vk.h": {
    "bytes": 12640,
    "sha256": "821f1434c8888db742310f09ce22571f3da4195afdf338c8d1c07f0296d8dce0"
  },
  "include/nvsdk_ngx_helpers_vk.h": {
    "bytes": 19995,
    "sha256": "754697d9faa93913985fc91fceae2073763461710d742a1f262ced2a7b6b62c9"
  },
  "include/nvsdk_ngx_params.h": {
    "bytes": 7860,
    "sha256": "51a1f94116cf82f95c63a2c197371ed22e08bddb09918d36cc6e2ac712753c80"
  },
  "include/nvsdk_ngx_vk.h": {
    "bytes": 39150,
    "sha256": "3be54d0103cce02174fd34912c60c9f40c73b5eec919e42613a2268c589c53ad"
  },
  "lib/Windows_x86_64/rel/nvngx_dlssnr.dll": {
    "bytes": 266424944,
    "sha256": "2bc19bc09a7b89dd44f73e2ac0108fcc586e2154bdd0830f242f4f040e28789f"
  }
}

def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def git_blob(path):
    value = hashlib.sha1()
    value.update(b"blob " + str(path.stat().st_size).encode("ascii") + b"\0")
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def matches(path, record):
    return (path.is_file() and path.stat().st_size == record["bytes"] and
            (sha256(path) == record["sha256"] if "sha256" in record
             else git_blob(path) == record["git_blob"]))


def download(url, target, maximum):
    request = urllib.request.Request(url, headers={"User-Agent": "RA2Toolbox-SDK-build-dependency"})
    count = 0
    with urllib.request.urlopen(request, timeout=120) as source, target.open("wb") as output:
        while chunk := source.read(1024 * 1024):
            count += len(chunk)
            require(count <= maximum, "Download exceeds the pinned size")
            output.write(chunk)


def base_destination(name):
    return "LICENSE-base-DLSS.txt" if name == "LICENSE.txt" else name


def expected_files(include_runtime):
    files = {base_destination(name): record for name, record in BASE_FILES.items()}
    files.update({name: record for name, record in NR_FILES.items()
                  if include_runtime or name != RUNTIME_PATH})
    return files


def sevenzip(command):
    if command:
        executable = shutil.which(command) or (str(Path(command).resolve()) if Path(command).is_file() else None)
    else:
        executable = next((found for name in ("7zz", "7z", "7za")
                           if (found := shutil.which(name))), None)
    require(executable, "7-Zip is required for the official .7z SDK package; pass --sevenzip PATH")
    return executable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="Private SDK directory, outside the source tree")
    parser.add_argument("--cache", type=Path, help="Download cache directory; defaults beside the SDK")
    parser.add_argument("--sevenzip", help="7zz/7z/7za executable")
    parser.add_argument("--include-runtime", action="store_true", help="Also prepare the pinned x64 NR DLL for local runtime testing")
    parser.add_argument("--verify-only", action="store_true", help="Verify an existing SDK without network access or extraction")
    args = parser.parse_args()
    destination = args.output.resolve()
    repository = Path(__file__).resolve().parents[2]
    require(destination != repository and repository not in destination.parents,
            "Keep the downloaded proprietary SDK outside this repository")
    expected = expected_files(args.include_runtime)
    missing = [name for name, record in expected.items() if not matches(destination / name, record)]
    if args.verify_only:
        require(not missing, "SDK files missing or different: " + ", ".join(missing))
        print(json.dumps({"output": str(destination), "files": len(expected), "verified": True,
                          "runtime_included": args.include_runtime}))
        return
    cache = (args.cache or destination.parent / "neural-sdk-download-cache").resolve()
    cache.mkdir(parents=True, exist_ok=True)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ra2-neural-sdk-", dir=destination.parent) as temporary:
        staging = Path(temporary)
        for name, record in BASE_FILES.items():
            relative = base_destination(name)
            if relative in NR_FILES:
                continue  # The matched NR overlay supplies this common header.
            if matches(destination / relative, record):
                continue
            target = staging / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            download(BASE_ORIGIN + name, target, record["bytes"])
            require(matches(target, record), "Base SDK Git blob differs: " + name)
        nr_missing = [name for name, record in NR_FILES.items()
                      if (args.include_runtime or name != RUNTIME_PATH)
                      and not matches(destination / name, record)]
        if nr_missing:
            executable = sevenzip(args.sevenzip)
            archive = cache / "rtx-remix-ngx_sdk_dlnr-2.7z"
            record = {"bytes": NR_ARCHIVE_BYTES, "sha256": NR_ARCHIVE_SHA256}
            if not matches(archive, record):
                pending = cache / (archive.name + ".download")
                try:
                    download(NR_ARCHIVE_URL, pending, NR_ARCHIVE_BYTES)
                    require(matches(pending, record), "Official NR package SHA-256 differs")
                    os.replace(pending, archive)
                finally:
                    pending.unlink(missing_ok=True)
            unpacked = staging / "nr"
            unpacked.mkdir()
            # Use exact pinned member names; do not extract unselected package files.
            subprocess.run([executable, "x", "-y", "-o" + str(unpacked), str(archive), *nr_missing],
                           check=True, stdout=subprocess.DEVNULL)
            for name in nr_missing:
                source = unpacked / name
                require(matches(source, NR_FILES[name]), "NR package member differs: " + name)
                target = staging / name
                target.parent.mkdir(parents=True, exist_ok=True)
                os.replace(source, target)
        # Validate the final view before replacing any existing SDK files.
        for name, record in expected.items():
            staged = staging / name
            require(matches(staged if staged.is_file() else destination / name, record),
                    "Incomplete SDK staging: " + name)
        for name in expected:
            staged = staging / name
            if not staged.is_file():
                continue
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(staged, target)
    require(all(matches(destination / name, record) for name, record in expected.items()),
            "SDK final verification failed")
    manifest = {"base_commit": BASE_COMMIT, "nr_archive_url": NR_ARCHIVE_URL,
                "nr_archive_bytes": NR_ARCHIVE_BYTES, "nr_archive_sha256": NR_ARCHIVE_SHA256,
                "runtime_included": args.include_runtime,
                "files": {name: {"bytes": (destination / name).stat().st_size,
                                  "sha256": sha256(destination / name)} for name in expected}}
    (destination / "RA2_NEURAL_SDK_MANIFEST.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(destination), "files": len(expected),
                      "verified": True, "runtime_included": args.include_runtime}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print("SDK preparation failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)

