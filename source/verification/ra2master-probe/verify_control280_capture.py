#!/usr/bin/env python3
"""Verify two control-related changes using capture contexts and private PE inputs.

This uses composite instruction fixtures: captured original prefix at 50B6F0,
matched bodies from the provided PE. It is not replay of the captured game,
not proof of live callback execution, event construction, or multiplayer.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("capture", type=Path)
    ap.add_argument("game", type=Path, nargs="+")
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    report = json.loads((args.capture / "02-control-on-code.json").read_text())
    changes = {int(c["va"], 16): c for c in report["changes"]}
    assert {a: (c["before"], c["enabled"]) for a, c in changes.items()} == {
        0x50B6F7: ("751D", "EB17"),
        0x6339BB: ("31", "57"),
        0x6378E1: ("AB", "D1"),
    }
    rows = []
    sample_checks = []
    for game in args.game:
        raw = game.read_bytes()
        sha = hashlib.sha256(raw).hexdigest()
        pe = pefile.PE(data=raw)
        image = pe.get_memory_mapped_image()
        base = pe.OPTIONAL_HEADER.ImageBase
        read = lambda va, size: image[va-base:va-base+size]
        c = changes[0x50B6F7]
        cb = bytes.fromhex(c["context_before"])
        ca = base + int(c["context_rva"], 16)
        body_suffix = cb[0x50B6F9-ca:]
        assert body_suffix == read(0x50B6F9, len(body_suffix))
        house = cb[0x50B6F0-ca:0x50B6F9-ca] + read(0x50B6F9, 0x50B724-0x50B6F9)
        c = changes[0x6339BB]
        cb = bytes.fromhex(c["context_before"])
        ca = base + int(c["context_rva"], 16)
        prefix = cb[0x6339B0-ca:]
        assert prefix == read(0x6339B0, len(prefix))
        helper = read(0x6339B0, 0x25)

        # The 6378E0 context is not a clean CALL boundary in either private PE.
        c = changes[0x6378E1]
        ctx = bytes.fromhex(c["context_before"])
        ctx_va = base + int(c["context_rva"], 16)
        exact_mismatches = [hex(ctx_va+i) for i, byte in enumerate(ctx)
                            if byte != read(ctx_va+i, 1)[0]]
        assert exact_mismatches == [hex(x) for x in range(0x6378E0, 0x6378E5)]
        call_blob = ctx[0x6378E0-ctx_va:0x6378E5-ctx_va]
        assert call_blob == read(0x637E40, 5) == bytes.fromhex("E8AB38EDFF")
        sample_checks.append({
            "sha256": sha,
            "captured_house_suffix_matches": True,
            "captured_index_helper_prefix_matches": True,
            "anomalous_context_mismatches_vs_private_pe": exact_mismatches,
            "same_five_bytes_at_valid_call": "0x637E40",
            "not_same_game_sha_as_capture": True,
        })
        for patch in ("baseline", "global_only", "global_and_index_helper"):
            for mode in (0, 3):
                for foreign in (False, True):
                    u = Uc(UC_ARCH_X86, UC_MODE_32)
                    u.mem_map(0x400000, 0x800000)
                    u.mem_map(0x20000000, 0x10000)
                    u.mem_map(0x30000000, 0x10000)
                    u.mem_write(0x50B6F0, house)
                    u.mem_write(0x6339B0, helper)
                    if patch != "baseline":
                        u.mem_write(0x50B6F7, bytes.fromhex("EB17"))
                    if patch == "global_and_index_helper":
                        u.mem_write(0x6339BB, bytes.fromhex("57"))
                    local, other, obj = 0x20000000, 0x20001000, 0x20002000
                    owner = other if foreign else local
                    put = lambda addr, value: u.mem_write(addr, struct.pack("<I", value))
                    put(0xA8B238, mode)
                    put(0xA83D4C, local)
                    put(local+0x30, 0)
                    put(other+0x30, 1)
                    u.mem_write(local+0x1EC, b"\1\0")
                    u.mem_write(other+0x1EC, b"\0\0")
                    put(obj+0x21C, owner)

                    def run(addr, ecx):
                        sp, stop = 0x30008000, 0x3000F000
                        put(sp, stop)
                        u.reg_write(UC_X86_REG_ESP, sp)
                        u.reg_write(UC_X86_REG_ECX, ecx)
                        u.emu_start(addr, stop, count=120)
                        assert u.reg_read(UC_X86_REG_ESP) == sp+4
                        return u.reg_read(UC_X86_REG_EAX)

                    can_control = run(0x50B6F0, owner)
                    effective_index = run(0x6339B0, obj)
                    expected_control = int(patch != "baseline" or not foreign)
                    expected_index = int(foreign and patch != "global_only")
                    assert (can_control, effective_index) == (expected_control, expected_index)
                    assert struct.unpack("<I", u.mem_read(obj+0x21C, 4))[0] == owner
                    assert struct.unpack("<I", u.mem_read(0xA83D4C, 4))[0] == local
                    rows.append({"sample_sha256": sha, "patch": patch, "game_mode": mode,
                                 "foreign": foreign, "controlled_result": can_control,
                                 "index_helper_result": effective_index,
                                 "owner_and_current_player_unchanged": True, "passed": True})
    result = {"evidence_level": "composite instruction fixture; not live-game replay",
              "sample_checks": sample_checks, "cases": rows,
              "passed": len(rows), "multiplayer_tested": False,
              "event_houseindex_traced": False}
    args.output.write_text(json.dumps(result, indent=2)+"\n")
    print(json.dumps({"passed": len(rows), "samples": len(sample_checks), "output": str(args.output)}))


if __name__ == "__main__":
    main()
