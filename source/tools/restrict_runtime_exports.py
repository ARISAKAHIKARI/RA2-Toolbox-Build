#!/usr/bin/env python3
"""Restrict Zig 0.13/MinGW's incidental libunwind DLL exports.

Zig 0.13's zig c++ driver rejects --exclude-all-symbols. The linker can
nonetheless automatically export libunwind's private symbols. This tool
performs a narrow PE export-directory edit, retaining all original project
export RVAs and ordinals. It never modifies executable code, imports or
resource sections. The full PE/ABI verifier runs afterwards.
"""
from pathlib import Path
import argparse
import json
import struct

from pe32_image import Image

REQUIRED = frozenset({
    "GameRuntimeHook", "GameRuntimePostHook", "GameRuntimeInfo",
    "AssistProtocolInfo", "AssistMappingPrefix", "ManageGameEntry",
    "GameEntryManagerInfo",
})
ALIASES = {
    "GameRuntimeHook@12": "GameRuntimeHook",
    "GameRuntimePostHook@12": "GameRuntimePostHook",
    "ManageGameEntry@4": "ManageGameEntry",
}


def sanitize(path: Path):
    original = path.read_bytes()
    pe = Image(original)
    if pe.machine != 0x14c or not (pe.characteristics & 0x2000):
        raise ValueError("Expected the x86 GameRuntime DLL")
    directory_rva, directory_size = pe.directory(0)
    if not directory_rva or directory_size < 40:
        raise ValueError("DLL export directory is missing")
    directory_at = pe.offset(directory_rva, 40)
    functions_count = pe.u32(directory_at + 20)
    names_count = pe.u32(directory_at + 24)
    functions_rva = pe.u32(directory_at + 28)
    names_rva = pe.u32(directory_at + 32)
    ordinals_rva = pe.u32(directory_at + 36)
    if not (0 < functions_count <= 65536 and 0 < names_count <= 65536):
        raise ValueError("Unreasonable export counts")
    functions_at = pe.offset(functions_rva, functions_count * 4)
    names_at = pe.offset(names_rva, names_count * 4)
    ordinals_at = pe.offset(ordinals_rva, names_count * 2)
    original_exports = pe.exports()
    if not REQUIRED <= original_exports.keys():
        raise ValueError("A required project export is missing")
    if not all(original_exports[a] == original_exports[b]
               for a, b in ALIASES.items() if a in original_exports):
        raise ValueError("A stdcall export alias points to a different function")

    items = []
    for index in range(names_count):
        name_rva = struct.unpack_from("<I", original, names_at + index * 4)[0]
        ordinal = struct.unpack_from("<H", original, ordinals_at + index * 2)[0]
        if ordinal >= functions_count:
            raise ValueError("Export ordinal outside function table")
        name = pe.cstring(name_rva)
        items.append((name, name_rva, ordinal))
    if [item[0] for item in items] != sorted(original_exports):
        raise ValueError("Malformed or unsorted export-name table")
    allowed = REQUIRED | ALIASES.keys()
    kept = [item for item in items if item[0] in allowed]
    extras = [item for item in items if item[0] not in allowed]
    if any(not (name.startswith("_Unwind_") or name.startswith("unw_"))
           for name, _, _ in extras):
        raise ValueError("Unexpected non-libunwind export; refusing to modify DLL")
    if not extras:
        print(json.dumps({"result": "already restricted", "names": len(kept)}))
        return
    if not kept or len(kept) != len(REQUIRED) + sum(a in original_exports for a in ALIASES):
        raise ValueError("Project export count does not match whitelist")
    keep_ordinals = {ordinal for _, _, ordinal in kept}
    remove_ordinals = {ordinal for _, _, ordinal in extras} - keep_ordinals

    modified = bytearray(original)
    # Leave public EAT indexes and their underlying code/data RVAs untouched.
    # Clear ONLY the incidental function-table slots to prevent ordinal access.
    for ordinal in remove_ordinals:
        struct.pack_into("<I", modified, functions_at + ordinal * 4, 0)
    # Loader binary-searches the names array, so retain original sorted order.
    for i in range(names_count):
        if i < len(kept):
            _, name_rva, ordinal = kept[i]
        else:
            name_rva, ordinal = 0, 0
        struct.pack_into("<I", modified, names_at + i * 4, name_rva)
        struct.pack_into("<H", modified, ordinals_at + i * 2, ordinal)
    struct.pack_into("<I", modified, directory_at + 24, len(kept))

    # Leave an absent PE checksum absent. Recompute a present checksum.
    checksum_at = pe.optional + 64
    if pe.u32(checksum_at):
        struct.pack_into("<I", modified, checksum_at, 0)
        checksum = 0
        padded = bytes(modified) + (b"\0" if len(modified) % 2 else b"")
        for offset in range(0, len(padded), 2):
            checksum += padded[offset] + 256 * padded[offset + 1]
            checksum = (checksum & 0xffff) + (checksum >> 16)
        checksum = (checksum & 0xffff) + (checksum >> 16)
        checksum += len(modified)
        struct.pack_into("<I", modified, checksum_at, checksum & 0xffffffff)

    result = Image(bytes(modified))
    current = result.exports()
    expected = {name: rva for name, rva in original_exports.items() if name in allowed}
    if current != expected:
        raise ValueError("Export restriction failed to preserve exact project RVAs")
    if result.imports() != pe.imports():
        raise ValueError("Import table changed unexpectedly")
    if result.resource_bytes() != pe.resource_bytes():
        raise ValueError("Resource directory changed unexpectedly")
    path.write_bytes(modified)
    print(json.dumps({
        "result": "restricted", "project_exports": sorted(current),
        "removed_libunwind_names": len(extras),
        "suppressed_ordinal_slots": len(remove_ordinals),
        "size_unchanged": len(modified) == len(original),
    }))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    sanitize(parser.parse_args().dll)
