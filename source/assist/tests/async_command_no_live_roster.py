"""Source contract: async command capture never secretly scans a live roster.

This is structural admission coverage, NOT in-game FPS or multiplayer proof.
The paired native build and actual worker DLL tests establish compilation.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / "source"
bridge = (root / "bridge.cpp").read_text()
worker = (root.parent.parent / "shared/source/world_frame_command_worker_win.h").read_text()
runtime = (root / "world_frame_runtime.h").read_text()
def between(s, left, right):
    assert left in s and right in s
    return s.split(left, 1)[1].split(right, 1)[0]
assert 'static bool prepareWorldIndex(int frame)' in bridge
gate = between(bridge, "static bool prepareWorldIndex(int frame){", '\n#include "../../shared/source/key_gesture.h"')
assert gate.index('worldFrameCommandWorker.enabled()') < gate.index('indexWorld(frame)')
assert 'return refreshWorldIndex(frame,false)' in gate
fallback = between(bridge, "static bool refreshWorldIndex(int frame,bool executionGuards){", "static bool prepareWorldIndex(")
assert 'if(!executionGuards||worldFrameCommandWorker.enabled())' in fallback
assert fallback.index('if(!executionGuards||worldFrameCommandWorker.enabled())') < fallback.rindex('indexWorld(frame)')
for path in ("sticky_bridge.h", "control_object_owner_bridge.h",
             "native_mega_gateway_bridge.h", "private_outlist_bridge.h"):
    body = (root / path).read_text()
    assert "prepareWorldIndex(frame)" in body, path
    assert "indexWorld(frame)" not in body, path
assert 'worldFrameCommandWorker.take(key,graphDemand' in runtime
assert 'if(asyncCommand)' in runtime
assert runtime.index('if(asyncCommand)') < runtime.index('worldframe::CaptureGraph<Live>capture(')
assert 'commandRequestGate.defer(key,graphDemand' in runtime
assert 'commandRequestGate.submitted(key,graphDemand' in runtime
assert runtime.index('commandRequestGate.defer(key,graphDemand') < runtime.index('requests.assign(hostRequests.begin()')
assert 'worldFrameCommandWorker.deferBusy(key,graphDemand,requestRevision,globalVehicles)' in runtime
assert runtime.index('worldFrameCommandWorker.deferBusy(key,graphDemand') < runtime.index('requests.assign(hostRequests.begin()')
assert 'coalesceInFlightWorld(lastRequestedKey_,key' in worker
assert 'commandRequestGate.reset()' in runtime
assert 'worldFrameCommandWorker.request(key,graphDemand' in runtime
assert 'CaptureGraph<decltype(checkedRead)>' in worker
# Native global magnetron admission must not scan 16K candidates on one frame.
native_orders = (root / "native_orders_bridge.h").read_text()
assert "globalVehicleScan.budget(total)" in native_orders
assert "globalVehicleScan.advance(total,examined)" in native_orders
assert "globalVehicleCandidateAt(n)" in native_orders
assert "globalVehicleCandidates[n]" not in native_orders
assert "memcpy(globalVehicleCandidates" not in bridge
# Unknown catalogue stamps cannot trigger an unbounded main-thread rescan.
assert "catalogueReady=false;scanTypes();" not in bridge
assert "A failed stamp read means UNKNOWN" in bridge
# Large Host repair results are staged in a bounded verifier, not sorted
# and authorized to issue native orders in one callback.
assert "std::sort(knownIDs" not in bridge
assert "repairResultGate.step(" in bridge
# Command admission is persistent: transient queue failure cannot consume a
# one-shot Ctrl+D Deploy. Carrier groups are hash linked, not 32K sorted.
native_orders = (root / "native_orders_bridge.h").read_text()
unload_policy = (root / "unload_admission.h").read_text()
assert "unloadretry::classify(result)" in native_orders
assert "unloadretry::committed(outcome)" in native_orders
assert "unloadRosterGroups.add(" in native_orders
assert "std::sort(unloadRoster" not in native_orders
assert "ReindexBudget=128" in unload_policy
assert "while(rebuildAt<count&&work++<ReindexBudget)" in unload_policy
assert "if(!worldFrameCommandWorker.enabled())indexWorld(frame);" in bridge


# The original manual token/pointer must reach the native EventClass
# gateway for delayed external, load and unload decisions.
proof = (root / "execution_proof.h").read_text()
autoload = (root / "autoload_bridge.h").read_text()
unload = (root / "unload_compute_bridge.h").read_text()
external = (root / "external_orders_bridge.h").read_text()
assert 'executionproof::verify(*proof' in bridge
assert 'executionproof::ordered(*proof' in bridge
assert 'static bool submitProvenMission(' in bridge
assert 'return nativeMission(house,id,mission,target,destination,destinationTag' in bridge
assert 'submitProvenMission(house,infantry,7,0,vehicle,0x34' in autoload
assert '{p.id,p.object,tokens[i]}' in autoload
assert '{v.id,v.object}' in autoload
assert 'job.manualToken=tokens[i];' in autoload
assert 'manualVersions.get(r.id)!=r.manualToken' in autoload
assert '{r.id,r.object,r.manualToken}' in autoload
assert 'enterEvent(house,r.id,r.vehicleID,proof,&ticket)' in autoload
assert 'submitProvenMission(house,r.id,decision.mission' in unload
assert '{s.id,s.pointer,s.token}' in unload
assert 'capturedTargetPointer=q.pointer;' in unload
assert 'submitProvenMission(house,o.unit,o.mission' in external
assert 'if(o.purpose==planner::MagnetronDropOrder){' in external
drop=external.split('if(o.purpose==planner::MagnetronDropOrder){',1)[-1].split('if(!submitProvenMission(',1)[0]
assert 'executionproof::verify(proof' in drop
assert 'nativeObjectEvent(' in drop
assert '{o.unit,o.unitPointer,o.token}' in external
assert '{o.target,o.targetPointer}' in external
assert 'a.sourcePointer=p;' in external
assert 'int admitted=admitPlannerOrder(o,house,sh->planMatch,sh->planTick);' in external
assert 'executionproof::Ticket proof{sourceMatch,house,sourceTick,' in external
assert 'return o.purpose==planner::OreHarvestOrder?admitPlannerOrder(o,house,match,0):-1;' in external
runtime_plan=(root / 'runtime_planner_bridge.h').read_text()
assert 'admitPlannerOrder(o,house,planningWorld.match,planningWorld.tick)' in runtime_plan
harvest=(root / 'ore_harvest_bridge.h').read_text()
assert 'order.unitPointer=j.pointer;' in harvest
planner_types=(root / 'planner_types.h').read_text()
planner_engine=(root / 'external_planner.h').read_text()
assert 'sourcePointer=0;' in planner_types
assert 'unitPointer=0,targetPointer=0,destinationPointer=0;' in planner_types
assert 'stampProvenance(o,' in planner_engine
provenance=(root / 'planner_provenance.h').read_text()
assert 'order.unitPointer=source?source->sourcePointer:0;' in provenance
assert 'order.targetPointer=target?target->sourcePointer:0;' in provenance
assert 'order.destinationPointer=dest?dest->sourcePointer:0;' in provenance
assert 'ManualTakeover' in proof and 'TargetChanged' in proof and 'WrongMatch' in proof
assert 'THREAD_PRIORITY_BELOW_NORMAL' in worker
assert 'WaitForSingleObject(thread_,0)' in worker
assert 'latestTicket_.fetch_add(1' in worker
print("PASS async command graph and four native Hook paths never sync-scan the full live roster; worker expiration is nonblocking")
