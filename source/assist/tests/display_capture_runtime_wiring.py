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
passive=bridge.split("static void capturePassiveWorld(",1)[1].split("static bool serviceReadOnlyRuntime(",1)[0]
assert "if(!connected)return;" in passive
assert "const uint32_t demand=worldFrameServer.demandMask();" in passive
assert "if(demand)captureWorldFrame(readLiveWorld,world,house,now,demand,true);" in passive
# Explicit read-only and view-only authority share the same strict boundary.
displayonly=bridge.split("static void captureDisplayOnlyWorld(",1)[1].split("static void capturePassiveWorld(",1)[0]
assert "const uint32_t demand=connected?worldFrameServer.demandMask():0u;" in displayonly
assert "if(demand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly))" in displayonly
assert "captureWorldFrame(readLiveWorld,world,house,now,demand,true);" in displayonly
assert "else retireWorldFrame();" in displayonly
readonly=bridge.split("static bool serviceReadOnlyRuntime(",1)[1].split("static void tick()",1)[0]
assert "captureDisplayOnlyWorld(world,house,now,wanted);" in readonly
# Duplicate retirement may be skipped only for an unchanged, zero-demand
# explicit read-only context. All authority transitions and new demand
# must invalidate the idle marker; the existing display helper is retained.
assert "if(!wasReadOnly)idleRetired.valid=false;" in readonly
assert "if(admission==readonly::Admission::Commands){\n  idleRetired.valid=false;" in readonly
assert "if(admission==readonly::Admission::Retiring){idleRetired.valid=false;retireWorldFrame();" in readonly
assert "if(demand!=0u){\n  idleRetired.valid=false;\n  captureDisplayOnlyWorld(world,house,now,wanted);" in readonly
assert "idleRetired.needsRetirement(sh->owner,match,worldFrameServer.connection(),worldProducerGeneration," in readonly
assert '#include "readonly_idle_retirement.h"' in bridge
assert "worldFrameReady||haveCaptureAttempt||worldFrameCandidateAvailable||" in readonly
assert "!runtimeWorldDemands.empty()||runtimeDemandOverflow" in readonly
assert "retireWorldFrame();\n  idleRetired={true,sh->owner,match,worldFrameServer.connection(),worldProducerGeneration};" in readonly
assert "if(transition!=sessionepoch::Transition::None){idleRetired.valid=false;retireWorldFrame();}" in readonly

assert "capturePassiveWorld(" not in readonly
assert "captureWorldFrame(" not in readonly
assert "if(authority==worldlife::Authority::ViewOnly)\n   captureDisplayOnlyWorld(worldSample,house,now,sharedWorldWanted);" in bridge
assert "else capturePassiveWorld(worldSample,house,now,sharedWorldWanted);" in bridge
assert "if(hostDisplayOnly)" in bridge
assert "else if(directRuntime&&!idleWithoutGraph)" in bridge
assert "const uint32_t hostDemand=sharedWorldWanted?worldFrameServer.demandMask():0u;" in bridge
idle=bridge.split("const bool idleWithoutGraph=",1)[1].split(";",1)[0]
# Between catalogue deadlines the catalogue implementation is irrelevant.
# A due graph refresh, pending tasks and any actual page demand still exclude idle.
assert "nativeCatalogue" not in idle
for condition in ("noCommandWork", "sh->externalDisplay", "!graphCatalogueDue", "worldDemand==uint32_t(worldframe::Base)", "!hostDemand", "runtimeWorldDemands.empty()", "!runtimeDemandOverflow"):
    assert condition in idle
assert "else if(!idleWithoutGraph&&(sharedWorldWanted||frameDue||cmd||resultsDue||fastDue||newMatch||hostFrameCaptureWanted(now)))" in bridge
assert "runtimeDirectActions=!worldFrameCommandWorker.enabled()&&sh->computeApplied==compute::All&&gameentry::active(gameentry::Client::Assist);" in bridge
assert "if(!runtimeDirectActions||commandRosterReady())pointerTick();" in bridge
direct_admission=bridge.split("const bool directRuntime=",1)[1].split(";",1)[0]
assert "sh->pointer.request==sh->pointer.ack" not in direct_admission
assert "AutoOreReturn|AntiTheft" in direct_admission
graph_jobs=bridge.split("const bool noGraphJobs=",1)[1].split(";",1)[0]
assert "scrollRefreshHz" not in graph_jobs and "scrollRefreshActive" not in graph_jobs
assert "!graphCatalogueDue" in direct_admission
assert "hostDemand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly)" in direct_admission
assert "(!hostDemand&&runtimeWorldDemands.empty()&&!runtimeDemandOverflow)" in direct_admission
assert "if(graphCatalogueDue)worldDemand|=worldframe::TypeCatalogue;" in bridge
assert "external::catalogue::captureVisit(readLiveWorld,stamp,collectProbe)" in bridge
assert "||scanTypes(true,&probes)" in bridge
assert "for(auto&batch:probes)batch.clear();" in bridge
assert "if(internal&&runtimeDirectActions)" in bridge
assert "if(runtimeDirectActions){if(!collect())return -12;}" in bridge
assert "allowDirect" not in runtime
assert "captureWorldFrame(readLiveWorld,worldSample,house,now,worldDemand,true);" in bridge

assert '#include "world_capture_requests.h"' in runtime
assert "if(boundaryOnly)requests.clear(); // Do not even EVALUATE Server::requests()." in runtime
assert "else assist::capturepolicy::prepareRequestedPages(false,key.tick," in runtime
assert "worldFrameServer.requests(),runtimeWorldDemands,runtimeDemandLeases,requests," in runtime
assert "&worldFrameCaptureScratch.warmSortScratch)" in runtime
assert "if(!boundaryOnly){runtimeWorldDemands.clear();runtimeDemandBits.fill(0);}" in runtime
assert "if(boundaryOnly){" in runtime
assert runtime.index("if(boundaryOnly){") < runtime.index("worldframe::CaptureGraph<Live>capture(")
assert "sh->worldSamplePages=0;sh->worldSampleReads=0;sh->worldSampleBytes=0;" in runtime
reuse=runtime.split("const bool reuseSync=",1)[1].split(";",1)[0]
for condition in ("!asyncCommand","worldFrameReady","haveCaptureAttempt","lastCaptureAttempt.sameFrame(key)","!(worldFrameBoundaryOnly&&!boundaryOnly)"):
    assert condition in reuse
assert "worldFrameServer.observe(key,boundaryOnly||reuseSync);" in runtime
assert "if(reuseSync||(asyncCommand&&reuseAsync))" in runtime
assert "const bool asyncCommand=!boundaryOnly&&worldFrameCommandWorker.enabled()" in runtime
assert "const bool stable=" in runtime and "before.same(after)" in runtime
assert "worldFrameServer.retire()" in runtime

# Exercise the actual tactical HUD subscriber, not just the generic reader.
# Base/Passengers/Combat are native demand bits: using them in sampleExternal
# silently defeats the display-only boundary even if ProcessReader is correct.
main=(root/"main.cpp").read_text(encoding="utf-8-sig")
passenger=(root/"passenger_draw.h").read_text(encoding="utf-8-sig")
display=main.split("uint32_t displayDemand=",1)[1].split("externalReader.setWorldDemand(displayDemand);",1)[0]
assert display.startswith("worldframe::HostVisualBase;")
assert "worldframe::HostVisualPassengers" in display
assert "worldframe::HostVisualCombat" in display
assert "worldframe::Passengers" not in display
assert "worldframe::Combat" not in display
assert "worldframe::Base" not in display
assert "source->read(0xA8ED84,&observedFrame,sizeof observedFrame)" in passenger
assert "observedFrame!=externalData->frame" in passenger
assert "source->key.tick!=externalData->sampledTick" in passenger

assert "worldClient_->visual(frame,worldDemand_" in reader
assert "if(worldFrame_&&worldFrame_->controlSource)worldClient_->requestVisual(worldDemands_,!worldComplete_,worldFrame_)" in reader
assert "VisualCache visuals_" in transport
assert "Token visual(" in transport
assert "WORLD_FRAME_TEST_VISUAL_CACHE" not in reader
assert "WORLD_FRAME_TEST_VISUAL_CACHE" not in transport
assert runtime.index("++d.syncGraphAttempts") < runtime.index("worldframe::CaptureGraph<Live>capture(")
assert runtime.index("++sh->sampling.boundaryAttempts") < runtime.index("++sh->sampling.boundaryCompleted")
assert runtime.index("++sh->sampling.reuseHits") < runtime.index("++d.syncGraphAttempts")
assert "sampling-admission pid=" in main
for counter in ("syncGraphAttempts", "syncGraphCompleted", "boundaryAttempts", "boundaryCompleted", "reuseHits", "graphHostDemand", "graphDisplayOnly", "graphNoHost", "graphCatalogue", "graphRequestedPages"):
    assert counter in runtime and counter in main
print("PASS production display split: Host VisualCache is live for display readers; passive overlays use boundary-only Runtime demand")

# A failed metadata stamp must not immediately rewalk the same type arrays.
# Preserve only the same-epoch catalogue, and discard it on identity change.
sampler=(external/"tactical_sampler.h").read_text(encoding="utf-8")
assert "catalogueDependencies_.capture(read_,stamp)" in sampler
assert "if(fresh||!catalogueAt_||uint32_t(now-catalogueAt_)>=uint32_t(catalogueReady_?500:100))" in sampler
assert "if(epoch!=sharedMatch_){catalogueDependencies_.clear();catalogueReady_=false;catalogueHash_=0;catalogueAt_=0;state_.typeCount=0;" in sampler
assert "house_=house;catalogueReady_=false;catalogueHash_=0;state_.typeCount=0;" in sampler
catalogue_refresh=sampler.split("if(catalogueDependencies_.capture(read_,stamp))",1)[1].split("state_.status=1;",1)[0]
assert catalogue_refresh.count("types();")==1
assert "types();" not in catalogue_refresh.split("}else{",1)[1]
assert "catalogueReady_=false;" in catalogue_refresh.split("}else{",1)[1]
print("PASS ordinary catalogue missing-data retry does not rewalk the unavailable source")
