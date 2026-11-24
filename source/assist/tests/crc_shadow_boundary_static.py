"""Static evidence contract for CRC Shadow simplification research.

This validates recorded source/disassembly relationships only. It does not prove
the complete Reveal call graph, Windows SEH behavior, or multiplayer safety.
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

bridge = read("assist/source/bridge.cpp")
crc_bridge = read("assist/source/crc_shadow_bridge.h")
crc_policy = read("assist/source/crc_shadow_policy.h")
crc_evidence = json.loads(read("research/evidence/crc-shadow-research.json"))
map_evidence = json.loads(read("research/evidence/trainer-map-cheatcheck.json"))
binary = json.loads(read("research/evidence/crc-shadow-single-store-disassembly.json"))

# Existing synchronous tool-owned Map::Reveal boundary.
start = bridge.index("else if(c==RevealMap){")
end = bridge.index("else if(c==OpenTech)", start)
branch = bridge[start:end]
begin_at = branch.index("crcMapToolBegin(house)")
reveal_at = branch.index("call<void>(0x577D90,0x87F7E8,(void*)house)")
end_at = branch.index("crcMapToolEnd()")
assert begin_at < reveal_at < end_at
assert branch.count("crcMapToolBegin(house)") == 1
assert branch.count("call<void>(0x577D90,0x87F7E8,(void*)house)") == 1
assert branch.count("crcMapToolEnd()") == 1
assert bridge.count("call<void>(0x577D90,0x87F7E8,(void*)house)") == 1

# Existing linear scan: three direct read candidates, seven direct write sites,
# with explicit unresolved alias/load/extension coverage gaps.
scan = crc_evidence["static_candidate_scan"]
assert len(scan["read_sites"]) == 3
assert all("CRC" in site.upper() for site in scan["read_sites"])
assert len(scan["write_sites"]) == 7
assert scan["unknowns"]

# Original SHA-matched PE disassembly confirms exactly one direct +0x241 store
# inside Map::Reveal and no direct read/call to ComputeGameCRC after that store.
assert binary["executable_sha256"] == crc_evidence["executable_sha256"]
assert binary["function"]["entry"] == "0x577D90"
assert binary["function"]["recorded_range"]["end_exclusive"] == "0x577F25"
assert binary["static_findings"]["direct_House_plus_0x241_accesses_in_MapReveal"] == ["0x577DB0"]
assert binary["static_findings"]["direct_ComputeGameCRC_calls_in_MapReveal"] == []
assert binary["static_findings"]["direct_House_plus_0x241_reads_after_store_in_MapReveal"] == []
assert binary["static_findings"]["indirect_calls_and_transitive_call_graph_exhaustive"] is False
readers = {item["address"]: item for item in binary["reader_classification"]}
assert readers["0x502FFF"]["classification"] == "CRC serializer input"
assert readers["0x64DCCA"]["classification"] == "ComputeGameCRC House MapIsClear input"
assert readers["0x64E250"]["classification"] == "diagnostic logging read"
assert "not an exhaustive proof" in binary["reader_scope_note"]
store = next(item for item in binary["relevant_instructions"] if item["address"] == "0x577DB0")
assert store["bytes"] == "889941020000"
assert store["instruction"] == "mov byte ptr [ecx+0x241],bl"
audit = binary["second_pass_control_flow_audit"]
assert audit["map_reveal_virtual_call"]["call_site"] == "0x577DF4"
assert audit["map_reveal_virtual_call"]["observed_default_singleton"]["slot_target"] == "0x5C0E30"
assert audit["map_reveal_virtual_call"]["observed_default_singleton"]["target_bytes"] == "32C0C3"
assert audit["null_house_helper_call"]["target_bytes"] == "C3"
assert audit["vision_field"]["actual_read"] == "0x577E0F"
assert binary["trampoline_codegen_probe"]["result"].startswith("PASS locally")
outer_crc = audit["outer_crc_scheduler"]
assert outer_crc["routine_entry"] == "0x647260"
assert outer_crc["direct_compute_game_crc_call_sites"] == ["0x64731C", "0x647684"]
assert "single-store design leaves House+0x241 physically unchanged" in outer_crc["relation_to_map_reveal"]

# The separate map-range evidence agrees with the SHA-matched disassembly.
reveal_range = next(
    item for item in map_evidence["unchanged_ranges"]
    if item["address"].lower() == "0x577d90"
)
lo = int(reveal_range["address"], 16)
hi = lo + int(reveal_range["length"])
writers = [int(item.split(":", 1)[0], 16) for item in scan["write_sites"]]
assert [address for address in writers if lo <= address < hi] == [0x577DB0]

# Production remains the old eight-site implementation until all gates pass.
assert "constexpr unsigned SiteCount=sizeof(Sites)/sizeof(Sites[0]);" in crc_policy
assert "crcMapSites[crcshadow::SiteCount]" in crc_bridge
assert "crcMapShadowTick();" in bridge
assert "if(!(sh->flags&ExperimentalReveal)||sh->renderReveal||" in crc_bridge
assert 'GetModuleHandleW(L"Ares.dll")||GetModuleHandleW(L"Phobos.dll")' in crc_bridge

# Research-only candidate scopes suppression to the exact House identity.
single = read("research/crc_shadow_single_store_candidate.h")
assert "currentHouse!=scope.house" in single
assert "epoch!=scope.epoch||currentHouseID!=scope.houseID" in single
assert "currentHouse!=scope.house" in single[single.index("inline bool abort"):]
assert "inline bool consumeSkip" in single
assert "pushfd" in single and "popfd" in single
assert "mov byte ptr [ecx+241h],bl" in single
assert "resume at the original next instruction 0x577DB6" in single
assert "Do not read," in single and "Visionary state" in single
single_code = "\\n".join(line.split("//", 1)[0] for line in single.splitlines())
assert "Cell" not in single_code and "Visionary" not in single_code and "0x240" not in single_code
assert "actual naked trampoline and Win32 SEH wiring are not implemented" in single

# Keep the zero-patch post-call restoration candidate as a comparison only.
transaction = read("research/crc_shadow_boundary_candidate.h")
assert "runtimeunwind::run(" in transaction
assert "Result::WriteFailed" in transaction and "Result::VerifyFailed" in transaction
assert "Result::IdentityChanged" in transaction

print("PASS: SHA-matched Map::Reveal single-store evidence; exact-House scoped candidate; current production remains unchanged")
