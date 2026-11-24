#!/usr/bin/env python3
"""Compile and inspect the x86 single-store trampoline sequence mirrored by production."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().with_name("crc_shadow_single_store_trampoline_probe.cpp")


def coff_text_section(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) < 20:
        raise AssertionError("truncated COFF object")
    machine, section_count, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", data, 0)
    if machine != 0x14C:
        raise AssertionError(f"expected i386 COFF, got machine=0x{machine:04X}")
    section_offset = 20 + optional_size
    for index in range(section_count):
        off = section_offset + index * 40
        name = data[off : off + 8].split(b"\0", 1)[0]
        raw_size = struct.unpack_from("<I", data, off + 16)[0]
        raw_ptr = struct.unpack_from("<I", data, off + 20)[0]
        if name == b".text":
            return data[raw_ptr : raw_ptr + raw_size]
    raise AssertionError("COFF .text section not found")


def main() -> None:
    compiler = shutil.which("clang++")
    objdump = shutil.which("objdump")
    if not compiler or not objdump:
        raise SystemExit("clang++ and GNU objdump are required for this code-generation probe")
    if not SOURCE.is_file():
        raise SystemExit(f"probe source missing: {SOURCE}")

    with tempfile.TemporaryDirectory(prefix="crc-single-store-") as tmp:
        obj = Path(tmp) / "crc-shadow-single-store.obj"
        subprocess.run(
            [compiler, "-target", "i686-pc-windows-msvc", "-fms-extensions", "-c", str(SOURCE), "-o", str(obj)],
            check=True,
        )
        text = coff_text_section(obj)
        relocations = subprocess.check_output([objdump, "-r", str(obj)], text=True)

        callback_prologue = bytes.fromhex(
            "9C 60 89 E3 81 EC 10 02 00 00 83 E4 F0 0F AE 04 24 FC DB E3 D9 2C 24"
        )
        callback_restore = bytes.fromhex(
            "FF 73 10 FF 73 18 E8 00 00 00 00 83 C4 08 89 43 10 0F AE 0C 24 89 DC 61 9D"
        )
        store_tail = bytes.fromhex(
            "9C 80 3D 00 00 00 00 00 75 06 88 99 41 02 00 00 "
            "C6 05 00 00 00 00 00 9D 68 B6 7D 57 00 C3"
        )
        for label, sequence in (
            ("GPR/flags/FP save prologue", callback_prologue),
            ("guarded callback and full state restore", callback_restore),
            ("conditional original store and exact continuation", store_tail),
        ):
            if sequence not in text:
                raise AssertionError(f"compiled x86 code is missing {label}")

        if relocations.count("_GuardedMapCrcRevealStore") != 1:
            raise AssertionError("expected exactly one guarded callback relocation")
        if relocations.count("_crcMapSkipStore") != 2:
            raise AssertionError("compare and clear must reference the same one-shot flag")

    print("CRC single-store x86 trampoline code-generation contract: PASS")
    print("  - preserves GPRs, EFLAGS, x87 and SSE state around the guarded callback")
    print("  - conditional branch skips exactly the original 6-byte MOV")
    print("  - clears the one-shot flag and restores original EFLAGS")
    print("  - resumes at original Map::Reveal continuation 0x577DB6")
    print("  - callback and both one-shot-flag relocations are present")
    print("  - production source is separately checked against this sequence by crc_shadow_boundary_static.py")


if __name__ == "__main__":
    main()
