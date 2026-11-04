#!/usr/bin/env python3
"""Exercise discovery with Win32 mocks and a synthetic, non-executable file.

No user game binary is read or copied. Only the public discovery fingerprint
fields are present in the sparse fixture; it cannot run as a PE program.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--out", type=Path, required=True)
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
    (out / "result.json").write_text(json.dumps({
        "status": "passed" if result.returncode == 0 else "failed",
        "evidence": "controlled Win32 mocks",
        "image": "synthetic non-executable discovery fingerprint",
        "build": build, "output": (result.stdout + result.stderr).strip(),
        "real_game_tested": False,
    }, ensure_ascii=False, indent=2) + "\n")
    print(result.stdout, end="")
    result.check_returncode()


if __name__ == "__main__":
    main()
