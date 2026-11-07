#!/usr/bin/env python3
"""Exercise discovery with Win32 mocks and a synthetic, non-executable file.

No user game binary is read or copied. Only the public discovery fingerprint
fields are present in the sparse fixture; it cannot run as a PE program.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baseline-ref", help="Git revision whose production discovery must reject the newly supported aliases")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    image = bytearray(4813161)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 60, 128)
    image[128:134] = bytes.fromhex("50 45 00 00 4c 01")
    image[152:154] = bytes.fromhex("0b 01")
    for at, value in ((168, 0x3CD80F), (180, 0x400000), (208, 0x794000)):
        struct.pack_into("<I", image, at, value)
    for at, value in (
        (0x2339B0, "8b 89 1c 02 00 00 56 8b 71 30 e8 31 7d ed ff 84"),
        (0x237E37, "8b 8f 1c 02 00 00 8b 71 30 e8 ab 38 ed ff 84 c0 74 0c a1 4c"),
        (0x3B8A00, "83 ec 20 53 8b 5c 24 2c 56 8b f1 85 db 0f 84 2c"),
        (0x42D028, "00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00"),
    ):
        data = bytes.fromhex(value)
        image[at:at + len(data)] = data
    fixture = out / "synthetic-discovery-image.bin"
    fixture.write_bytes(image)
    binary = out / "random-process-discovery"
    build = [args.cxx, "-std=c++17", "-O2", "-UNDEBUG", "-pthread",
             "-I" + str(root / "tests/winmock"),
             str(root / "tests/random_process_discovery.cpp"), "-o", str(binary)]
    subprocess.run(build, check=True)
    result = subprocess.run([str(binary), str(fixture)],
                            capture_output=True, text=True)
    (out / "test.log").write_text(result.stdout + result.stderr)
    result.check_returncode()
    checks = json.loads(result.stdout.strip().splitlines()[-1])
    baseline = None
    if args.baseline_ref:
        old = subprocess.check_output(
            ["git", "show", args.baseline_ref + ":source/external_reader/source/process_discovery.h"],
            cwd=root.parent, text=True)
        original_sha256 = hashlib.sha256(old.encode()).hexdigest()
        # Keep the old production body unchanged; only resolve its relative
        # dependency includes from this isolated temporary output directory.
        for relative in ("../../shared/source/message_query.h", "process_image_identity.h"):
            absolute = (root / "external_reader/source" / relative).resolve().as_posix()
            old = old.replace('"' + relative + '"', '"' + absolute + '"')
        subject = out / "baseline-process-discovery.h"
        subject.write_text(old)
        old_binary = out / "baseline-alias-discovery"
        old_build = [args.cxx, "-std=c++17", "-O2", "-UNDEBUG", "-pthread",
                     "-I" + str(root / "tests/winmock"),
                     "-DDISCOVERY_NAME_REGRESSION_ONLY",
                     '-DDISCOVERY_HEADER="' + subject.as_posix() + '"',
                     str(root / "tests/random_process_discovery.cpp"), "-o", str(old_binary)]
        subprocess.run(old_build, check=True)
        old_run = subprocess.run([str(old_binary), str(fixture)], capture_output=True, text=True)
        (out / "baseline.log").write_text(old_run.stdout + old_run.stderr)
        assert old_run.returncode == 1, "Old production discovery must reproduce the missing-alias failure"
        assert "gameares.exe" in old_run.stdout, "The actual reported gameares.exe name must fail in the old source"
        assert "alias_failures=5" in old_run.stdout, "Every missing alias must fail through actual old production discovery"
        baseline = {"ref": args.baseline_ref, "production_source_sha256": original_sha256,
                    "exit_code": old_run.returncode, "build": old_build,
                    "expected_rejected_cases": 5,
                    "output": (old_run.stdout + old_run.stderr).strip()}
    (out / "result.json").write_text(json.dumps({
        "status": "passed" if result.returncode == 0 else "failed",
        "evidence": "controlled Win32 mocks",
        "image": "synthetic non-executable discovery fingerprint",
        "build": build, "output": (result.stdout + result.stderr).strip(),
        "real_game_tested": False,
        "baseline": baseline,
        "checks": checks,
        "source_sha256": {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in (
                              root / "external_reader/source/process_discovery.h",
                              root / "external_reader/source/game_process_names.h",
                              root / "external_reader/source/process_file_description.h",
                              root / "external_reader/source/process_target_selection.h",
                              root / "tests/random_process_discovery.cpp")},
    }, ensure_ascii=False, indent=2) + "\n")
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
