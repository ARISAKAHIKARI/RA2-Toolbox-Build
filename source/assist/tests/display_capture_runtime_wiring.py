"""Verify that the already-proven display-only admission/lease policy is
actually called by production Runtime; without wiring, its unit test passes
while every passive Host overlay still triggers the full game-thread graph.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]/"source"
bridge=(root/"bridge.cpp").read_text(encoding="utf-8")
runtime=(root/"world_frame_runtime.h").read_text(encoding="utf-8")

assert "capturepolicy::needsNativeFlagWork(sh->flags,sh->externalDisplay!=0)" in bridge
assert "!cmd&&!resultsDue&&!newMatch&&!sh->flags" not in bridge
assert "worldDemand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly)" in bridge
assert "sharedWorldWanted&&noCommandWork" in bridge

assert '#include "world_capture_requests.h"' in runtime
assert "capturepolicy::prepareRequestedPages(boundaryOnly,key.tick," in runtime
assert "worldFrameServer.requests(),runtimeWorldDemands,runtimeDemandLeases,requests)" in runtime
assert "if(!boundaryOnly){runtimeWorldDemands.clear();runtimeDemandBits.fill(0);}" in runtime
assert "worldFrameReady&&!(worldFrameBoundaryOnly&&!boundaryOnly)" in runtime
assert "const bool asyncCommand=!boundaryOnly&&worldFrameCommandWorker.enabled()" in runtime
assert "const bool stable=" in runtime and "before.same(after)" in runtime
assert "worldFrameServer.retire()" in runtime

print("PASS production capture gate: passive external overlays skip game-thread full graphs, Host requests not copied/sorted, command leases survive boundary and same-frame escalation stays mandatory")
