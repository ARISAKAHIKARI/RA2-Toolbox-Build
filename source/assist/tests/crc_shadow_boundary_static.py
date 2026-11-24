"""Static evidence and production-source contract for CRC single-store suppression.

This validates recorded PE evidence and the source wiring. It does not prove
complete transitive call-graph coverage, Windows SEH behavior, or multiplayer safety.
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


bridge = read("assist/source/bridge.cpp")
crc_bridge = read("assist/source/crc_shadow_bridge.h")
crc_policy = read("assist/source/crc_shadow_policy.h")
single_policy = read("assist/source/crc_shadow_single_store_policy.h")
guard = read("assist/source/guard.cpp")
main = read("assist/source/main.cpp")
crc_evidence = json.loads(read("research/evidence/crc-shadow-research.json"))
map_evidence = json.loads(read("research/evidence/trainer-map-cheatcheck.json"))
binary = json.loads(read("research/evidence/crc-shadow-single-store-disassembly.json"))

# Existing synchronous tool-owned Map::Reveal boundary.
start = bridge.index("else if(c==RevealMap){")
end = bridge.index("else if(c==OpenTech)", start)
branch = bridge[start:end]
begin_at = branch.index("crcMapToolBegin(house)")
reveal_at = branch.index("call<void>(0x577D90,0x87F7E8,(void*)house)")
scope_end_at = branch.index("crcMapToolEnd(completed)")
assert begin_at < reveal_at < scope_end_at
assert branch.count("crcMapToolBegin(house)") == 1
assert branch.count("call<void>(0x577D90,0x87F7E8,(void*)house)") == 1
assert branch.count("crcMapToolEnd(completed)") == 1
assert "runtimeunwind::run(" in branch
assert "completed=true" in branch
assert "static void crcMapToolEnd(bool)" in bridge

# Existing linear scan: three direct read candidates, seven direct write sites,
# with explicit unresolved alias/load/extension coverage gaps.
scan = crc_evidence["static_candidate_scan"]
assert len(scan["read_sites"]) == 3
assert all("CRC" in site.upper() for site in scan["read_sites"])
assert len(scan["write_sites"]) == 7
assert scan["unknowns"]

# SHA-matched PE disassembly: one direct MapIsClear store inside Map::Reveal.
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

# Production has one scoped Map::Reveal store hook, not the old eight-site mirror.
assert '#include "crc_shadow_single_store_policy.h"' in crc_bridge
assert "crcshadow::Sites[6]" in crc_bridge
assert "crcMapRevealSingleEntry" in crc_bridge
assert "GuardedMapCrcRevealStore" in crc_bridge
assert "crcMapSkipStore" in crc_bridge
assert "push 577DB6h" in crc_bridge
assert "mov byte ptr [ecx+241h],bl" in crc_bridge
assert "crcMapTargetIndex" in crc_bridge
assert "memcmp(actual,crcMapSite.patch,site.size)!=0" in crc_bridge
assert "!crcMapInstalled" in crc_bridge
assert "crcMapModel" not in crc_bridge
assert "crcMapSites[crcshadow::SiteCount]" not in crc_bridge
assert "crcshadow::restoreData" not in crc_bridge
assert crc_bridge.count("gameentry::patch(") == 2  # one install, one restore
assert "crcMapShadowTracked=crcMapInstalled?1u:0u" in crc_bridge
assert "crcMapShadowSubstitutions=crcMapSuppressed" in crc_bridge
assert "crcMapToolEnd(bool completed)" in crc_bridge
abort_start = crc_bridge.index('extern "C" void __cdecl AssistMapCrcRevealAbort(')
abort_end = crc_bridge.index("// Preserve the original machine state", abort_start)
assert "crcMapFault(false)" in crc_bridge[abort_start:abort_end]
assert "crcMapPublish" not in crc_bridge[abort_start:abort_end]
assert "if(!crcMapFaulted)crcMapPublish()" in crc_bridge
assert "crcshadowsingle::end(crcMapSingleScope)" in crc_bridge

# The policy is the production implementation used by the compiled unit test.
assert "inline Result begin(" in single_policy
assert "inline Result beforeStore(" in single_policy
assert "inline bool abort(" in single_policy
assert "inline void end(" in single_policy
assert "currentHouse!=scope.house" in single_policy
assert "epoch!=scope.epoch||currentHouseID!=scope.houseID" in single_policy

# Guarded callback uses the existing x86 SEH boundary; outer finally owns scope cleanup.
assert "GuardedMapCrcRevealStore(unsigned house,unsigned savedEBX)" in guard
assert "AssistMapCrcRevealStore(house,savedEBX)" in guard
assert "AssistMapCrcRevealAbort(house)" in guard
assert "RuntimeScopedCall(void* context" in guard
assert "__finally {cleanup(context);}" in guard

# CRC production path never reads or mirrors Cell/Visionary state.
production_code = "\\n".join(line.split("//", 1)[0] for line in crc_bridge.splitlines())
assert "0x240" not in production_code and "Visionary" not in production_code and "Cell" not in production_code

# Preserve admission gates for the experimental multiplayer reveal path.
assert "crcMapShadowTick();" in bridge
assert "if(!(sh->flags&ExperimentalReveal)||sh->renderReveal||" in crc_bridge
assert 'GetModuleHandleW(L"Ares.dll")||GetModuleHandleW(L"Phobos.dll")' in crc_bridge

# The old candidate remains research-only; production uses the policy above.
candidate = read("research/crc_shadow_single_store_candidate.h")
assert "actual naked trampoline and Win32 SEH wiring are not implemented" in candidate
assert "单点抑制" in main or "crcShadowSuppressed" in main

print("PASS: SHA-matched Map::Reveal evidence; one-site production trampoline; exact-House scope; SEH finally cleanup; no Cell/Visionary mirror")
