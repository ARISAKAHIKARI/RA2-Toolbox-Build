"""Verify that the already-proven display-only admission/lease policy is
actually called by production Runtime; without wiring, its unit test passes
while every passive Host overlay still triggers the full game-thread graph.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]/"source"
bridge=(root/"bridge.cpp").read_text(encoding="utf-8")
runtime=(root/"world_frame_runtime.h").read_text(encoding="utf-8")
shared=root.parent.parent/"shared"/"source"
external=root.parent.parent/"external_reader"/"source"
transport=(shared/"world_frame_transport.h").read_text(encoding="utf-8")
reader=(external/"process_reader.h").read_text(encoding="utf-8")

assert "capturepolicy::needsNativeFlagWork(sh->flags,sh->externalDisplay!=0)" in bridge
assert "!cmd&&!resultsDue&&!newMatch&&!sh->flags" not in bridge
assert "worldDemand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly)" in bridge
assert "sharedWorldWanted&&noCommandWork" in bridge

assert '#include "world_capture_requests.h"' in runtime
assert "if(boundaryOnly)requests.clear(); // Do not even EVALUATE Server::requests()." in runtime
assert "else assist::capturepolicy::prepareRequestedPages(false,key.tick," in runtime
assert "worldFrameServer.requests(),runtimeWorldDemands,runtimeDemandLeases,requests," in runtime
assert "&worldFrameCaptureScratch.warmSortScratch)" in runtime
assert "if(!boundaryOnly){runtimeWorldDemands.clear();runtimeDemandBits.fill(0);}" in runtime
assert "if(boundaryOnly){" in runtime
assert runtime.index("if(boundaryOnly){") < runtime.index("worldframe::CaptureGraph<Live>capture(")
assert "sh->worldSamplePages=0;sh->worldSampleReads=0;sh->worldSampleBytes=0;" in runtime
assert "worldFrameReady&&!(worldFrameBoundaryOnly&&!boundaryOnly)" in runtime
assert "const bool asyncCommand=!boundaryOnly&&worldFrameCommandWorker.enabled()" in runtime
assert "const bool stable=" in runtime and "before.same(after)" in runtime
assert "worldFrameServer.retire()" in runtime

assert "if(frame)frame=worldClient_->visual(frame,worldDemand_" in reader
assert "worldClient_->requestVisual(worldDemands_,!worldComplete_)" in reader
assert "VisualCache visuals_" in transport
assert "Token visual(" in transport
assert "WORLD_FRAME_TEST_VISUAL_CACHE" not in reader
assert "WORLD_FRAME_TEST_VISUAL_CACHE" not in transport
print("PASS production display split: Host VisualCache is live for display readers; passive overlays use boundary-only Runtime demand")
