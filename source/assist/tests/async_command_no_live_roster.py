"""Source contract: Runtime owns the same-frame action index; no feature duplicates the roster scan.

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
assert 'if(worldFrameCommandWorker.enabled())return refreshWorldIndex(frame,false);' in gate
assert 'indexWorld(frame);' in gate
assert 'return indexedFrame==frame;' in gate
fallback = between(bridge, "static bool refreshWorldIndex(int frame,bool executionGuards=true){", "// Identity-index publication")
assert 'if(worldFrameCommandWorker.enabled()){' in fallback
assert 'SampleReadScope sample;indexWorld(frame);' in fallback
assert 'if(!executionGuards)return false;' in fallback
assert 'indexWorld(frame);' in fallback
# No active feature may rediscover the full Techno list through a live Vec.
for path in ("bridge.cpp", "type_select_bridge.h", "autoload_bridge.h",
             "burst_bridge.h", "combat_probe_bridge.h",
             "cooperative_bridge.h", "native_orders_bridge.h"):
    body = (root / path).read_text()
    assert "vec(0xA8EC78" not in body, path
main = (root / "main.cpp").read_text()
process_reader = (root.parent.parent / "external_reader/source/process_reader.h").read_text()
display_demand = between(main, "const bool passengerOverlay=", "external::WorldFrameScope worldScope")
assert "worldframe::Resources" not in display_demand
assert "HostVisual" not in display_demand
assert "worldframe::Passengers" in display_demand
assert "worldframe::Combat" in display_demand
# Host VisualCache is production display sampling, not a test-only branch.
assert "WORLD_FRAME_TEST_VISUAL_CACHE" not in process_reader
assert "if(frame)frame=worldClient_->visual(frame,worldDemand_" in process_reader
assert "worldClient_->requestVisual(worldDemands_,!worldComplete_)" in process_reader
assert "if(worldframe::visualDemand(worldDemand_))" in process_reader
assert "else worldClient_->request(worldDemands_,!worldComplete_);" in process_reader
assert "sceneReady?SceneKeyBit:0u" in worker

assert 'CaptureGraph' not in fallback
assert 'static void indexWorld(int frame)' in bridge
for path in ("sticky_bridge.h", "control_object_owner_bridge.h",
             "native_mega_gateway_bridge.h", "private_outlist_bridge.h"):
    body = (root / path).read_text()
    assert "prepareWorldIndex(frame)" in body, path
    assert "indexWorld(frame)" not in body, path
# Page requests are bounded, deduplicated and ownership-transferred. A partial
# or oversized dependency set must never be submitted as a complete graph.
assert "requests.assign(hostRequests.begin()" not in runtime
assert "requests.insert(requests.end()" not in runtime
assert "runtimeDemandLeases.visit(key.tick,appendPage)" in runtime
assert "requests.size()>=worldframe::MaxPages" in runtime
assert "runtimeWorldDemands.size()>=worldframe::MaxPages" in runtime
assert "runtimeDemandOverflow" in runtime
assert "queued_.pages.swap(pages)" in worker
assert "job.pages.swap(self->queued_.pages)" in worker
assert "bool reclaimRequestBuffer(std::vector<uint32_t>&pages)" in worker
assert "worldFrameCommandWorker.reclaimRequestBuffer(requests)" in runtime
assert runtime.index("worldFrameCommandWorker.reclaimRequestBuffer(requests)") < runtime.index("const auto&hostRequests=worldFrameServer.requests();")
# The full CaptureGraph runs synchronously in Runtime by default. The
# optional command worker is a benchmark-only opt-in; AsyncPublisher remains
# the immutable Host transport and bounded missing-page recovery worker.
assert '#elif defined(WORLD_FRAME_TEST_SYNC_CAPTURE)' not in runtime
assert runtime.count('world_frame_async_win.h') == 1
assert 'using WorldFramePublication=worldframe::AsyncPublisher<>' in runtime
assert 'The game thread owns world capture, indexing and command admission.' in runtime
assert 'if(!worldFrameServer.wanted(key.owner,key.tick))' in runtime
assert 'candidateCount&&worldFrameServer.wanted(key.owner,key.tick)' in runtime
assert 'worldFrameCommandWorker.take(key,graphDemand' in runtime
assert 'if(asyncCommand)' in runtime
assert runtime.index('if(asyncCommand)') < runtime.index('worldframe::CaptureGraph<Live>capture(')
assert 'commandRequestGate.defer(key,graphDemand' in runtime
assert 'commandRequestGate.submitted(key,graphDemand' in runtime
assert runtime.index('commandRequestGate.defer(key,graphDemand') < runtime.index('auto&requests=worldFrameCaptureRequests;')
assert 'worldFrameCommandWorker.deferBusy(key,graphDemand,requestRevision,globalVehicles)' in runtime
assert runtime.index('worldFrameCommandWorker.deferBusy(key,graphDemand') < runtime.index('auto&requests=worldFrameCaptureRequests;')
# Game-thread lock contention must defer request assembly instead of
# scanning 16K dependencies and attempting a second try-lock submission.
busy = between(worker, 'bool deferBusy(const Key&key,', '\n bool request(const Key&key,')
assert 'std::try_to_lock' in busy
assert 'if(!lock.owns_lock())return true;' in busy
assert 'if(stopping_.load(std::memory_order_acquire))' in busy
assert 'return thread_&&WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0;' in busy
assert 'if(!launch())return false;' in worker  # exited worker can relaunch
# Candidate index is allowed across frames, but each native UID lookup
# *outside* a sealed pure planner still checks live identity.
candidate_gate = between(bridge, 'static bool verifyCandidateID(', 'static bool refreshWorldIndex(')
assert 'readLiveWorld(pointer+0x10' in candidate_gate
assert 'if(sampleReadDepth)' in candidate_gate

assert 'coalesceInFlightWorld(lastRequestedKey_,key' in worker
# The command worker is opt-in; default product execution is synchronous.
assert 'RA2TOOLBOX_ASYNC_COMMAND_CAPTURE' in worker
assert 'GetEnvironmentVariableA' in worker
assert 'WORLD_FRAME_TEST_SYNC_CAPTURE' in worker
assert 'return true;' in worker
assert 'worldFrameCommandWorker.enabled()' in runtime
assert 'commandRequestGate.reset()' in runtime
assert 'worldFrameCommandWorker.request(key,graphDemand' in runtime
# The Host demand list is loaded with Server::requests(); that call can update
# demandRevision. A job stamped with the earlier revision can never be taken,
# even with identical bytes and a matching game frame.
assert runtime.index('const auto&hostRequests=worldFrameServer.requests();') < runtime.index('const uint64_t assembledRevision=')
assert runtime.index('const uint64_t assembledRevision=') < runtime.index('worldFrameCommandWorker.request(key,graphDemand,assembledRevision')
assert 'commandRequestGate.submitted(key,graphDemand,assembledRevision,globalVehicles)' in runtime
assert 'worldFrameCommandWorker.take(key,graphDemand,requestRevision' in runtime
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
assert "external::catalogue::capture(" not in bridge
assert "if(sample&&sampleWorldSource->catalogue.complete)" in bridge
catalogue = bridge.split("static bool scanTypes(){",1)[1].split('#include "sidebar.h"',1)[0]
assert "if(!sampleReadDepth||!sampleReadComplete)return false;" in catalogue
assert "readLiveWorld(" not in catalogue and "memcpy(d.id" not in catalogue
assert catalogue.index("if(!sampleReadComplete)return false;") < catalogue.index("std::copy_n(next.types,count,snap.types)")
assert "stampTypeCatalogue(self->staging_)" in worker
assert "now-worldFrameCatalogueProbeAt" in bridge
assert "if(graphDemand&worldframe::TypeCatalogue)worldFrameCatalogueProbeAt=key.tick;" in runtime
# Large Host repair results are staged in a bounded verifier, not sorted
# and authorized to issue native orders in one callback.
assert "std::sort(knownIDs" not in bridge
assert "repairResultGate.step(" in bridge
# Command admission is persistent: transient queue failure cannot consume a
# one-shot Ctrl+D Deploy. Carrier groups are hash linked, not 32K sorted.
native_orders = (root / "native_orders_bridge.h").read_text()
unload_policy = (root / "unload_admission.h").read_text()
if "unloadCarrierKnowledge(task,house)" in native_orders:
    # pre14 missing-page state machine retains the same one-shot ticket.
    assert "if(!knowledge)continue;" in native_orders
    assert "if(result==2)" in native_orders and "if(result==1)" in native_orders
    assert "if(!task.attempted)" in native_orders
    assert "unloadPassengerKnowledge" in native_orders
else:
    assert "unloadretry::classify(result)" in native_orders
    assert "unloadretry::committed(outcome)" in native_orders
    assert "unloadRosterGroups.add(" in native_orders
    assert "std::sort(unloadRoster" not in native_orders
assert "ReindexBudget=128" in unload_policy
assert "while(rebuildAt<count&&work++<ReindexBudget)" in unload_policy
assert "if(worldFrameCommandWorker.enabled())return refreshWorldIndex(frame,false);" in bridge
assert bridge.count("indexWorld(") == 4

# A missed strict capture must pause indexed work without aborting the
# observation publication or consuming that frame's indexed input refresh.
dispatcher = bridge.split("withInputPublication(sh,match,now,frame,observationFrame,[&]{", 1)[1]
assert "if(deferIndexedWork){++sh->worldSampleFailures;++sh->sampling.indexUnavailable;}" in dispatcher
assert "externalInputFrame=!deferIndexedWork&&" in dispatcher
assert "if(externalInputFrame){indexedInputMatch=match;indexedInputFrame=frame;}" in dispatcher
assert "if(!deferIndexedWork){superweaponGrantTick(house,frame,now);consumeSubmittedIntents(house,frame,now);}" in dispatcher
assert "captureCooperativeFrame(now,!deferIndexedWork);" in dispatcher
assert "publishPlanningWorld(house,frame,now,!deferIndexedWork);" in dispatcher


# The original manual token/pointer must reach the native EventClass
# gateway for delayed external, load and unload decisions.
proof = (root / "execution_proof.h").read_text()
autoload = (root / "autoload_bridge.h").read_text()
unload = (root / "unload_compute_bridge.h").read_text()
external = (root / "external_orders_bridge.h").read_text()
local_fallback = between(dispatcher, "else{\n  uiSoundCommand(now);", "nativePlacementTick(now);")
assert "eventdelivery::isSingleplayer(sessionMode)" in local_fallback
assert "candidateRosterReady()" in local_fallback
assert "externalOrdersTick(house,frame,now,true);" in local_fallback
assert "if(sh->pointer.request!=sh->pointer.ack)pointerTick(true);" in local_fallback
assert "serviceCooperativePlan(house,frame,now,true,preciseNow);" in local_fallback
coop = (root / "cooperative_bridge.h").read_text()
coop_plan = between(coop, "static void serviceCooperativePlan(", "static void cooperativeTick(")
assert "if(sh->cooperativeMode==cooperative::Enter&&!candidateEntryCohortReady())return;" in coop_plan
assert "int budget=candidateOnly?(enter?1:4):cooperative::MaxSources" in coop_plan
assert "identities->boarding(order.id,order.target)" in coop_plan
assert "!proof||!candidateEntryCapacity(t,p,o.id,house,c,slots,reserved)" in coop
assert "preciseNow-cooperativeCandidateAt<CooperativeCandidateIntervalUs" in coop_plan
assert "identities->attack(order.id,order.target)" in coop_plan
assert "mode==cooperative::AirSingle,false,proof" in coop
assert "if(result==-30)return;\n     loadTaskSeen=completed;finish(result);" in autoload
candidate = between(external, "else if(crossFrame){", "else {\n  while")
assert "candidateRosterReady()||sampleReadDepth||!simulationReady()" in candidate
assert "planBatch.candidatePass(count,4,preciseclock::micros(),worldFrameCandidateVersion" in candidate
assert "manualVersions.get(o.unit)!=o.token" in candidate
assert "actorID!=o.unit||targetID!=o.target" in candidate
assert "worldByID(o.unit)!=o.unitPointer||worldByID(o.target)!=o.targetPointer)return 0;" in candidate
assert "admitPlannerOrder(o,house,sh->planMatch,sh->planTick,true)" in candidate
assert "planningWorld.frame<=sh->planFrame" in candidate
assert "result=outgoing::ReplanFromNewObservation;" in candidate
assert "if(result<0)while(!planBatch.done(count)" in external
assert "if(accepted&(1u<<i))kind=planner::Receipt::Accepted;" in main
assert "else if((rejected&(1u<<i))&&shared->planResult!=-7)kind=planner::Receipt::Rejected;" in main
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

# Host-only page demand may refresh the worker's read-only publication without
# revoking an already sealed, same-frame Runtime command sample.
capture = runtime.split("template<class Live>static bool captureWorldFrame", 1)[1].split(
    "static bool ensureWorldFrame", 1
)[0]
assert "worldframe::commandSampleReusable(" in capture
assert "worldframe::CaptureGraph<Live>capture(" in capture
assert "worldFramePublication.publish(worldFrame,worldFrameServer)" in capture
assert "worldFrameReady&&!worldFrameBoundaryOnly&&!runtimeDemandOverflow" in capture
assert "if(!strictReusable){worldFrameReady=false;worldFrameBoundaryOnly=false;}" in capture
assert "return strictReusable;" in capture

print("PASS Runtime same-frame index, final live validation, opt-in worker and separate Host display sampling contract")
