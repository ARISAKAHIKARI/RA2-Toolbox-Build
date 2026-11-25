"""Source contract for Runtime-owned action capture and independent Host display sampling.

The synchronous Runtime capture path is the product default. The old full-graph
worker remains an explicit opt-in benchmark only; it never performs a live
roster scan on the game thread while enabled.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "source"
bridge = (root / "bridge.cpp").read_text(encoding="utf-8")
policy = (root / "compute_policy.h").read_text(encoding="utf-8")
protocol = (root / "protocol.h").read_text(encoding="utf-8")
main = (root / "main.cpp").read_text(encoding="utf-8")
planner = (root / "runtime_planner_bridge.h").read_text(encoding="utf-8")
runtime = (root / "world_frame_runtime.h").read_text(encoding="utf-8")
worker = (root.parent.parent / "shared/source/world_frame_command_worker_win.h").read_text(encoding="utf-8")
transport = (root.parent.parent / "shared/source/world_frame_transport.h").read_text(encoding="utf-8")
visual = (root.parent.parent / "shared/source/world_frame_visual.h").read_text(encoding="utf-8")
process_reader = (root.parent.parent / "external_reader/source/process_reader.h").read_text(encoding="utf-8")


def between(source, left, right):
    assert left in source and right in source
    return source.split(left, 1)[1].split(right, 1)[0]


# All action-producing planners and their initial shared state belong to Runtime.
assert "constexpr uint32_t Fixed=All;" in policy
assert "computeRequested=compute::Fixed,computeApplied=compute::Fixed" in protocol
assert "p.drop=extra.magnetronDrop&&!compute::inside(shared->computeApplied,compute::MagnetronDrop)" in main
assert "p.observeDrop=extra.magnetronDrop&&!compute::inside(shared->computeApplied,compute::MagnetronDrop)" in main
assert "runtimeActionSampling" in main
assert "runtimeFrameDemand||sh->planRequest!=sh->planAck" in bridge
assert "indexedFrame!=frame||!commandRosterReady()" in planner

# Command capture workers are opt-in. Default product capture seals a same-frame
# Runtime graph; the async branch returns before CaptureGraph and cannot live-scan.
assert 'GetEnvironmentVariableA("RA2TOOLBOX_ASYNC_COMMAND_CAPTURE"' in worker
assert "const bool asyncCommand=!boundaryOnly&&worldFrameCommandWorker.enabled();" in runtime
assert "if(asyncCommand){" in runtime
async_branch = between(runtime, "if(asyncCommand){", "if(!worldFramePublication.begin(worldFrame))")
assert "worldFrameCommandWorker.take(key,graphDemand" in async_branch
assert "worldFrameCommandWorker.request(key,graphDemand" in async_branch
assert "indexWorld(" not in async_branch
assert "worldframe::CaptureGraph<Live>capture(" not in async_branch
assert "worldframe::CaptureGraph<Live>capture(" in runtime
assert "using WorldFramePublication=worldframe::AsyncPublisher<>" in runtime
assert "worldFramePublication.publish(worldFrame,worldFrameServer)" in runtime

# The synchronous Runtime index is same-frame only and every final native lookup
# re-reads the live UID. The worker path keeps its immutable roster fast path.
assert "static void indexWorld(int frame)" in bridge
assert "return indexedFrame==snap.frame&&indexedCount>=0" in bridge
prepare = between(bridge, "static bool prepareWorldIndex(int frame){", '\n#include "../../shared/source/key_gesture.h"')
assert "if(worldFrameCommandWorker.enabled())return refreshWorldIndex(frame,false);" in prepare
assert "indexWorld(frame);" in prepare
refresh = between(bridge, "static bool refreshWorldIndex(int frame,bool executionGuards=true){", "// Identity-index publication")
assert "SampleReadScope sample;indexWorld(frame);complete=sample.complete();" in refresh
assert "if(!executionGuards)return false;" in refresh
assert "readLiveWorld(pointer+0x10,&observed,sizeof observed)" in bridge

# Host-only visual demand never enters Runtime command authorization. The Host
# VisualCache may lag and is read-only; active display-only demand emits a
# zero-page boundary instead of a full game-thread graph.
assert "VisualCache visuals_" in transport
assert "worldClient_->visual(frame,worldDemand_" in process_reader
assert "if(worldframe::visualDemand(worldDemand_))" in process_reader
assert "worldClient_->requestVisual(worldDemands_,!worldComplete_)" in process_reader
assert "else worldClient_->request(worldDemands_,!worldComplete_)" in process_reader
assert "uint32_t(now-latest_->visualTick)<visualReuseWindow(latest_->pages.size())" in visual
assert "pages<=256?10u:16u" in visual
assert "uint32_t(now-attemptedAt_)<16" in visual
assert "if(hostDisplayOnly)" in bridge
assert "captureWorldFrame(readLiveWorld,worldSample,house,now,worldDemand,true);" in bridge
assert "if(boundaryOnly){" in runtime
assert "worldFrame.resetBoundary(key)" in runtime

# No recurring display/automation route may secretly walk the full live
# Techno array. The explicit current-weapon selection shortcut is exceptional:
# it runs once on the native game thread, when pressed, and uses the original
# normal-weapon resolver (including Ares extended slots).
for path in ("autoload_bridge.h", "burst_bridge.h", "combat_probe_bridge.h",
             "cooperative_bridge.h", "native_orders_bridge.h"):
    assert "vec(0xA8EC78" not in (root / path).read_text(encoding="utf-8"), path
variant = (root / "type_select_bridge.h").read_text(encoding="utf-8")
assert "static int selectVariant(uint32_t house,bool global)" in variant
assert variant.count("vec(0xA8EC78") == 1
assert "normalVariantWeapon(p,type)" in variant
assert "get<uint32_t>(c.pointer+0x10)" not in variant
assert "read(c.pointer+0x10,&currentID,sizeof currentID)" in variant

# Page demand is bounded, deduplicated and ownership-transferred; a partial or
# oversized dependency set is never submitted as a complete graph.
assert "runtimeDemandLeases.visit(key.tick,appendPage)" in runtime
assert "requests.size()>=worldframe::MaxPages" in runtime
assert "runtimeDemandOverflow" in runtime
assert "worldFrameCommandWorker.reclaimRequestBuffer(requests)" in runtime
assert "commandRequestGate.defer(key,graphDemand" in runtime
assert "worldFrameCommandWorker.deferBusy(key,graphDemand,requestRevision,globalVehicles)" in runtime
assert "if(!lock.owns_lock())return true;" in worker

print("PASS Runtime action capture is synchronous by default; async capture is opt-in; Host VisualCache remains a separate read-only lane")
