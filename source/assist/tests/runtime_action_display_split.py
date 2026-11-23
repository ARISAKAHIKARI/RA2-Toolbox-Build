"""Source contract for Runtime-owned actions and Host-owned low-urgency display sampling."""
from pathlib import Path

source_root = Path(__file__).resolve().parents[2]
assist = source_root / "assist" / "source"
shared = source_root / "shared" / "source"
external = source_root / "external_reader" / "source"

policy = (assist / "compute_policy.h").read_text(encoding="utf-8")
protocol = (assist / "protocol.h").read_text(encoding="utf-8")
main = (assist / "main.cpp").read_text(encoding="utf-8-sig")
runtime = (assist / "world_frame_runtime.h").read_text(encoding="utf-8")
bridge = (assist / "bridge.cpp").read_text(encoding="utf-8")
planner = (assist / "runtime_planner_bridge.h").read_text(encoding="utf-8")
transport = (shared / "world_frame_transport.h").read_text(encoding="utf-8")
reader = (external / "process_reader.h").read_text(encoding="utf-8")
visual = (shared / "world_frame_visual.h").read_text(encoding="utf-8")
world = (shared / "world_frame.h").read_text(encoding="utf-8")
worker = (shared / "world_frame_command_worker_win.h").read_text(encoding="utf-8")

assert "constexpr uint32_t Fixed=All;" in policy
assert "computeRequested=compute::Fixed,computeApplied=compute::Fixed" in protocol
assert "p.drop=extra.magnetronDrop&&!compute::inside(shared->computeApplied,compute::MagnetronDrop)" in main
assert "p.observeDrop=extra.magnetronDrop&&!compute::inside(shared->computeApplied,compute::MagnetronDrop)" in main

# Runtime owns the same-frame roster. The async worker is optional only.
assert "static void indexWorld(int frame)" in bridge
assert "return indexedFrame==snap.frame&&indexedCount>=0" in bridge
assert bridge.count("indexWorld(") == 4
assert "if(worldFrameCommandWorker.enabled())return refreshWorldIndex(frame,false);" in bridge
assert "readLiveWorld(pointer+0x10,&observed,sizeof observed)" in bridge
assert "indexedFrame!=frame||!commandRosterReady()" in planner

assert "RA2TOOLBOX_ASYNC_COMMAND_CAPTURE" in worker
assert "GetEnvironmentVariableA" in worker
assert "const bool asyncCommand=!boundaryOnly&&worldFrameCommandWorker.enabled()" in runtime
assert "using WorldFramePublication=worldframe::AsyncPublisher<>" in runtime
assert "worldframe::CaptureGraph<Live>capture(" in runtime
assert "if(!worldFramePublication.begin(worldFramePublicationBuffer))return;" in runtime
assert "worldFramePublicationBuffer.copyForPublication(worldFrame)" in runtime
assert "if(requested.empty()){" in runtime
assert "boundaryKey.demand|=worldframe::BoundaryOnlyKeyBit" in runtime
assert "worldFramePublication.begin(worldFrame)" not in runtime
assert "bool copyForPublication(const Snapshot&source)" in world
assert "No production synchronous full-graph path exists" not in runtime
assert "WORLD_FRAME_TEST_SYNC_CAPTURE" not in runtime

# Display sampling is independent, read-only and may reuse a recent same-world frame.
assert "if(frame&&!frame->controlSource)frame=worldClient_->visual(frame,worldDemand_" in reader
assert "worldClient_->requestVisual(worldDemands_,!worldComplete_)" in reader
assert "VisualCache visuals_" in transport
assert "Token visual(" in transport
assert "WORLD_FRAME_TEST_VISUAL_CACHE" not in reader
assert "WORLD_FRAME_TEST_VISUAL_CACHE" not in transport
assert "uint32_t(now-latest_->visualTick)<32" in visual

print("PASS Runtime action / Host display split contract")
