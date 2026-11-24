"""Static evidence contract for the CRC single-boundary candidate.

This verifies source/evidence relationships only. It does not prove a complete
Map::Reveal execution, exhaustive xrefs, or multiplayer compatibility.
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

# There is already one synchronous tool-owned Reveal transaction boundary.
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

# The prior static scan found three +0x241 readers, all classified as CRC
# or CRCEngine paths; it also explicitly lists incomplete-xref limitations.
scan = crc_evidence["static_candidate_scan"]
assert len(scan["read_sites"]) == 3
assert all(("CRC" in site.upper()) for site in scan["read_sites"])
assert len(scan["write_sites"]) == 7
assert scan["unknowns"], "Do not silently promote the scan to exhaustive xrefs"

# Only one of the seven direct +0x241 writes lies inside the recorded
# Map::Reveal function range. This is the critical single-boundary premise.
reveal_range = next(
    item for item in map_evidence["unchanged_ranges"]
    if item["address"].lower() == "0x577d90"
)
lo = int(reveal_range["address"], 16)
hi = lo + int(reveal_range["length"])
writers = [int(item.split(":", 1)[0], 16) for item in scan["write_sites"]]
inside = [address for address in writers if lo <= address < hi]
assert inside == [0x577DB0], [hex(address) for address in inside]

# Current production remains the eight-site design; this audit does not
# silently remove it before the full native Reveal proof is available.
assert "constexpr unsigned SiteCount=sizeof(Sites)/sizeof(Sites[0]);" in crc_policy
assert "crcMapSites[crcshadow::SiteCount]" in crc_bridge
assert "crcMapShadowTick();" in bridge
assert "if(!(sh->flags&ExperimentalReveal)||sh->renderReveal||" in crc_bridge
assert 'GetModuleHandleW(L"Ares.dll")||GetModuleHandleW(L"Phobos.dll")' in crc_bridge

# Candidate uses a single save/call/compare-and-restore transaction with
# an SEH-finally boundary and retryable failure ownership.
candidate = read("research/crc_shadow_boundary_candidate.h")
assert "runtimeunwind::run(" in candidate
assert "if(live!=1)" in candidate and "Result::Conflict" in candidate
assert "Result::WriteFailed" in candidate and "Result::VerifyFailed" in candidate
assert "Result::IdentityChanged" in candidate

print("PASS: one existing tool Reveal boundary; one writer in Map::Reveal; three CRC-classified readers; xref gaps retained")
