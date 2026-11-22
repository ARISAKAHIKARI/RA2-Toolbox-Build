#pragma once
#include "../../shared/source/world_frame_capture.h"
#if defined(WORLD_FRAME_TEST_SYNC_CAPTURE) || defined(WORLD_FRAME_PLATFORM_FIXTURE)
#include "../../shared/source/world_frame_recovery.h"
#endif
#include "../../shared/source/world_frame_transport.h"
#include "world_capture_requests.h"
#if defined(WORLD_FRAME_TEST_SHADOW) && !defined(WORLD_FRAME_PLATFORM_FIXTURE)
#include "../../shared/source/world_frame_shadow_win.h"
#endif
#include <cstdio>

static worldframe::Snapshot worldFrame;
static worldframe::Server worldFrameServer;
#if defined(WORLD_FRAME_PLATFORM_FIXTURE)
// Existing synchronous transport fixtures keep their explicit execution seam.
struct WorldFramePublication {
 bool begin(worldframe::Snapshot&){return true;}
 void invalidate(){}
 bool stop(){return true;}
 bool publish(const worldframe::Snapshot&frame,const worldframe::Server&){return worldFrameServer.publish(frame);}
 bool requestRecovery(const worldframe::Key&,const worldframe::RequestedSpan*,size_t){return false;}
 size_t recovered(const worldframe::Key&,worldframe::RequestedSpan*,size_t){return 0;}
};
#elif defined(WORLD_FRAME_TEST_SYNC_CAPTURE)
// The former independent publisher/recovery worker exists only in the
// isolated synchronous regression DLL. Product Runtime never compiles it.
#include "../../shared/source/world_frame_async_win.h"
using WorldFramePublication=worldframe::AsyncPublisher<>;
#else
// Production has exactly one world-capture worker: CommandCaptureWorker.
// The old AsyncPublisher is not a dormant compatibility path in the product.
struct WorldFramePublication {
 bool begin(worldframe::Snapshot&){return false;}
 void invalidate(){}
 bool stop(){return true;}
 bool publish(const worldframe::Snapshot&,const worldframe::Server&){return false;}
 template<class...Args>bool requestRecovery(Args&&...){return false;}
 template<class...Args>size_t recovered(Args&&...){return 0;}
};
#endif
static WorldFramePublication worldFramePublication;
// The game frame keeps execution authority. The worker is the only production
// CaptureGraph owner; a strict same-frame handoff is required before native
// command use. No production synchronous fallback is compiled.
#ifdef WORLD_FRAME_PLATFORM_FIXTURE
struct WorldFrameCommandFixture {
 static bool enabled(){return false;}
 void invalidate(){}
 bool stop(){return true;}
 bool deferBusy(const worldframe::Key&,uint32_t,uint64_t,bool=false){return false;}
 bool reclaimRequestBuffer(std::vector<uint32_t>&){return false;}
 bool request(const worldframe::Key&,uint32_t,uint64_t,std::vector<uint32_t>&,bool=false){return false;}
 bool take(const worldframe::Key&,uint32_t,uint64_t,worldframe::Snapshot&,worldframe::CommandRoster* =nullptr,bool=false){return false;}
 bool takeObservation(const worldframe::Key&,uint32_t,worldframe::Snapshot&,worldframe::CommandRoster&,bool=false){return false;}
};
using WorldFrameCommandWorker=WorldFrameCommandFixture;
#else
using WorldFrameCommandWorker=::worldframe::CommandCaptureWorker;
#endif
static WorldFrameCommandWorker worldFrameCommandWorker;
static worldframe::CommandRoster worldFrameCommandRoster;
// Cross-frame observation is never the native command-sample authority.
// Own separately: the strict worldFrame/SampleReadScope can only expose
// same-frame sealed bytes, while this candidate index is bounded in age
// and every actual game order must check the live object's UID and House.
static worldframe::Snapshot worldFrameCandidate;
static worldframe::CommandRoster worldFrameCandidateRoster;
static bool worldFrameCandidateAvailable=false;
static uint32_t worldFrameCandidateVersion=0;
// Only an explicitly observational scope may read the cross-frame candidate.
// Never alias the strict worldFrame key or grant it native command authority.
static const worldframe::Snapshot* sampleWorldSource=nullptr;
static uint32_t sampleScopeTick=0;
// Coalesce identical in-flight requests before copying dependency pages.
static worldframe::CommandRequestGate commandRequestGate;
#if defined(WORLD_FRAME_TEST_SHADOW) && !defined(WORLD_FRAME_PLATFORM_FIXTURE)
static worldframe::ShadowGraphWorker worldFrameShadow;
#else
// The second full-graph reader is deliberately absent from production.
// Keep only a zero-cost seam for shutdown and test code that shares headers.
struct WorldFrameShadowDisabled {
 void invalidate(){}
 bool stop(){return true;}
 template<class...Args>bool request(Args&&...){return false;}
 template<class...Args>bool poll(Args&&...){return false;}
};
static WorldFrameShadowDisabled worldFrameShadow;
#endif
static uint32_t worldFrameSerial=0;static uint32_t worldFrameCapturedDemand=0;static uint64_t worldFrameCapturedRevision=0;static uint32_t runtimeDemandRevision=1;static unsigned sampleReadDepth=0;static bool sampleReadComplete=true;
static std::array<uint64_t,8192>runtimeDemandBits{},worldFrameRequestBits{};
static std::vector<uint32_t>runtimeWorldDemands;
static bool runtimeDemandOverflow=false,runtimeDemandBufferPrepared=false,worldFrameRequestBufferPrepared=false;
// The legacy targeted-recovery ring exists only in the synchronous test
// fixture. Production retries missing pages through the next complete graph.
#if defined(WORLD_FRAME_TEST_SYNC_CAPTURE) || defined(WORLD_FRAME_PLATFORM_FIXTURE)
static std::array<worldframe::RequestedSpan,256>runtimeMissingSpans{};
static uint32_t lastRecoverySubmitAt=0;
#endif
static bool worldFrameReady=false;
static DWORD worldFrameCatalogueProbeAt=0;
// Boundary-only sources are retained for isolated regression fixtures only.
// Product builds suspend HostDisplayOnly until its fields join the canonical
// complete Runtime snapshot.
static bool worldFrameBoundaryOnly=false;
static worldframe::DemandLeases runtimeDemandLeases;
static worldframe::CaptureScratch worldFrameCaptureScratch{};static std::vector<uint32_t>worldFrameCaptureRequests;
static worldframe::Key lastCaptureAttempt{};static bool haveCaptureAttempt=false;
static uint32_t failedCaptureAt=0,captureRetryDelay=0;
static uint64_t worldProducerGeneration=0;
static void requestWorldRange(uint32_t at,size_t n){
 if(!worldframe::range(at,n)||runtimeDemandOverflow)return;
 if(!runtimeDemandBufferPrepared){
  runtimeWorldDemands.reserve(worldframe::MaxPages);
  runtimeDemandBufferPrepared=true;
 }
 const uint32_t tick=sampleReadDepth?sampleScopeTick:worldFrame.key.tick;
 for(uint32_t page=at&~4095u;uint64_t(page)<uint64_t(at)+n;page+=4096){
  const uint32_t index=page>>12;
  if((index>>6)>=runtimeDemandBits.size()){runtimeDemandOverflow=true;break;}
  const uint64_t mask=uint64_t(1)<<(index&63);
  const auto retained=runtimeDemandLeases.retainStatus(page,tick);
  if(retained==worldframe::DemandLeases::RetainResult::Full||
     retained==worldframe::DemandLeases::RetainResult::Invalid){
   runtimeDemandOverflow=true;break;
  }
  const bool known=(runtimeDemandBits[index>>6]&mask)!=0;
  if(retained==worldframe::DemandLeases::RetainResult::Added&&known)
   if(!++runtimeDemandRevision)++runtimeDemandRevision; // an expired lease was re-added
  if(!known){
   // Keep the lease even if the per-sample vector is full. Overflow is
   // reported and no partial graph is submitted; later lease expiry or a
   // reduced demand set can make a complete request possible again.
   if(runtimeWorldDemands.size()>=worldframe::MaxPages){
    runtimeDemandOverflow=true;
    if(retained==worldframe::DemandLeases::RetainResult::Added)
     if(!++runtimeDemandRevision)++runtimeDemandRevision;
    break;
   }
   runtimeDemandBits[index>>6]|=mask;runtimeWorldDemands.push_back(page);
   if(retained==worldframe::DemandLeases::RetainResult::Added)
    if(!++runtimeDemandRevision)++runtimeDemandRevision;
  }
 }
}
// The worker can deliver a complete observational graph after the game
// frame advanced. Only read-only planners explicitly opting in may consume
// its sealed bytes, and no native EventClass is ever issued within a scope.
#if defined(WORLD_FRAME_PLATFORM_FIXTURE) || (defined(WORLD_FRAME_FIXTURE_ONLY) && !defined(WORLD_FRAME_TEST_CANDIDATES))
// Legacy transport fixtures have no background command worker and no
// Host/House runtime metadata. Their original strict-snapshot behavior is
// intentionally unchanged; native Win32 tests cover real worker candidates.
static bool observationalFrameReady(){return false;}
#else
static bool observationalFrameReady(){
 if(!worldFrameCandidateAvailable||!sh||!worldFrameCandidate.sealed()||
    worldFrameCandidate.overflow()||!worldFrameCandidateRoster.objects||
    !worldFrameCandidate.key.sameFrame(worldFrameCandidateRoster.key)||
    !worldFrameCandidateRoster.current(worldFrameCandidate.key)||
    worldFrameCandidateRoster.global!=bool(sh->magnetronGlobal))return false;
 auto now=worldFrameCandidate.key;
 now.pid=GetCurrentProcessId();now.owner=sh->owner;
 now.match=match;now.house=lastHouse;now.frame=snap.frame;now.session=snap.session;
 now.connectionLow=uint32_t(worldFrameServer.connection());
 now.connectionHigh=uint32_t(worldFrameServer.connection()>>32);
 now.generationLow=uint32_t(worldProducerGeneration);
 now.generationHigh=uint32_t(worldProducerGeneration>>32);
 // One full 2.8MB graph can consume most of the worker's 120ms budget.
 // 150ms would leave a 90ms feature black-out between successive 120ms
 // scans. Allow two worker cycles plus margin for pure observation only.
 // Native EventClass still requires current live identity/mission proof.
 return worldframe::acceptObservationCandidate(worldFrameCandidate.key,now,
          worldframe::Base,worldframe::Base,GetTickCount(),280);
}
#endif
static bool readWorldSample(uint32_t at,void*out,size_t n){
 requestWorldRange(at,n); // Keep successful consumer dependencies subscribed.
 const auto*source=sampleWorldSource?sampleWorldSource:&worldFrame;
 const bool ok=source&&source->sealed()&&source->read(at,out,n);
 if(!ok)sampleReadComplete=false;
 return ok;
}
class SampleReadScope {
 unsigned previousDepth_;bool previousComplete_,ready_,finished_=false,result_=false;
 const worldframe::Snapshot* previousSource_;
 uint32_t previousTick_;
public:
 explicit SampleReadScope(bool allowObservation=false):
    previousDepth_(sampleReadDepth),previousComplete_(sampleReadComplete),
    ready_(false),previousSource_(sampleWorldSource),previousTick_(sampleScopeTick){
  const bool strict=worldFrameReady&&!worldFrameBoundaryOnly&&worldFrame.sealed()&&
       worldFrame.key.match==match&&worldFrame.key.frame==snap.frame;
  const bool observation=allowObservation&&!strict&&observationalFrameReady();
  ready_=strict||observation;
  sampleWorldSource=strict?&worldFrame:(observation?&worldFrameCandidate:nullptr);
  sampleScopeTick=GetTickCount();
  ++sampleReadDepth;sampleReadComplete=ready_;
 }
 explicit operator bool()const{return ready_;}
 bool complete()const{return finished_?result_:ready_&&sampleReadComplete;}
 void finish(){if(finished_)return;result_=complete();sampleReadDepth=previousDepth_;
  sampleReadComplete=previousComplete_&&(!previousDepth_||result_);
  sampleWorldSource=previousSource_;sampleScopeTick=previousTick_;finished_=true;
 }
 ~SampleReadScope(){finish();}
 SampleReadScope(const SampleReadScope&)=delete;SampleReadScope&operator=(const SampleReadScope&)=delete;
};
// Opt-in ONLY for pure observation/forecasting; keep the default
// SampleReadScope strict for destructive state pruning and native authority.
class ObservationReadScope:public SampleReadScope {
public: ObservationReadScope():SampleReadScope(true){}
};
static const ::assist::ObjectIndex* sampledObjectIndex(){
 const auto& roster=sampleWorldSource==&worldFrameCandidate?worldFrameCandidateRoster:worldFrameCommandRoster;
 return sampleWorldSource&&roster.current(sampleWorldSource->key)?roster.objects.get():nullptr;
}
// Copy worker atomics to fixed-size IPC at 4 Hz. No native memory reads,
 // allocations, locks, file I/O or world traversal occur here.
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
static void publishSamplingWorkerDiagnostics(uint32_t now){
 static uint32_t publishedAt=0,lastHostPublicationSerial=0;
 if(!sh||!worldFrameCommandWorker.enabled()||
    (publishedAt&&uint32_t(now-publishedAt)<250))return;
 publishedAt=now;
 const auto d=worldFrameCommandWorker.diagnostics();
 auto&v=sh->sampling;
 v.workerStarted=d.started;v.workerPublished=d.published;
 v.workerExact=d.exact;v.workerCandidate=d.candidate;
 v.workerStale=d.stale;v.workerWorldChanged=d.worldChanged;
 v.workerBudget=d.budget;v.workerRosterFailed=d.rosterFailed;
 v.workerGraphIncomplete=d.incomplete;v.workerRejected=d.rejected;
 v.workerLastDurationMs=d.lastDurationMs;v.workerMaxDurationMs=d.maxDurationMs;
 v.workerLastDurationUs=d.lastDurationUs;
 v.workerLastCalls=d.lastCalls;v.workerLastBytes=d.lastBytes;
 v.workerLastPages=d.lastPages;
 v.workerFirstFrame=d.firstFrame;v.workerLastFrame=d.lastFrame;
 // pre.6 counted successfully built worker snapshots but hid Host starvation:
 // only rare same-frame take() updated worldSample*. The worker now directly
 // publishes sealed read-only graphs to Host IPC. Reflect the *actual*
 // successful transport publications in the existing seqlocked diagnostics.
 const auto host=worldFrameCommandWorker.hostPublication();
 if(host.serial&&host.serial!=lastHostPublicationSerial&&host.match==match){
  lastHostPublicationSerial=host.serial;
  InterlockedIncrement(&sh->worldSampleSeq);
  sh->worldSampleFrame=host.frame;
  sh->worldSamplePages=host.pages;
  sh->worldSampleReads=host.calls;
  sh->worldSampleBytes=host.bytes;
  sh->worldSampleRosters=1;
  sh->worldSampleSerial=host.serial;
  MemoryBarrier();InterlockedIncrement(&sh->worldSampleSeq);
 }
}
#endif
static void retireWorldFrame(){worldFrameCatalogueProbeAt=0;worldFrameCommandWorker.invalidate();commandRequestGate.reset();worldFrameCommandRoster.invalidate();worldFrameCandidateRoster.invalidate();worldFrameCandidateAvailable=false;worldFrameShadow.invalidate();worldFrameCapturedDemand=0;worldFrameCapturedRevision=0;worldFramePublication.invalidate();worldFrameBoundaryOnly=false;if(worldProducerGeneration&&++worldProducerGeneration==0)++worldProducerGeneration;worldFrameReady=false;haveCaptureAttempt=false;captureRetryDelay=0;worldFrameServer.retire();runtimeWorldDemands.clear();runtimeDemandLeases.clear();worldFrameCaptureRequests.clear();runtimeDemandBits.fill(0);worldFrameRequestBits.fill(0);runtimeDemandOverflow=false;
#if defined(WORLD_FRAME_TEST_SYNC_CAPTURE) || defined(WORLD_FRAME_PLATFORM_FIXTURE)
 runtimeMissingSpans.fill({});lastRecoverySubmitAt=0;
#endif
}
template<class Live>static bool captureWorldFrame(Live live,const worldlife::Sample&before,uint32_t house,DWORD now,uint32_t demand,bool permitDisplayBoundary=false){
 const bool boundaryOnly=permitDisplayBoundary&&demand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly);
 const bool asyncCommand=!boundaryOnly&&worldFrameCommandWorker.enabled();
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
 if(sh){
  ++sh->sampling.captureCalls;
  if(asyncCommand)publishSamplingWorkerDiagnostics(GetTickCount());
 }
#endif
 bool globalVehicles=false;
#ifndef WORLD_FRAME_PLATFORM_FIXTURE
 if(asyncCommand)globalVehicles=sh&&sh->magnetronGlobal;
#endif
 const uint32_t graphDemand=demand&~uint32_t(worldframe::HostDisplayOnly);
 const uint32_t captureDemand=boundaryOnly?demand:(demand&~uint32_t(worldframe::HostDisplayOnly));
 worldframe::Key key{GetCurrentProcessId(),sh?sh->owner:0,match,0,GetTickCount(),0,before.scenario,before.cells,house,uint32_t(before.count),before.frame,before.session};
 if(!worldProducerGeneration){LARGE_INTEGER stamp{};QueryPerformanceCounter(&stamp);worldProducerGeneration=uint64_t(stamp.QuadPart);if(!worldProducerGeneration)worldProducerGeneration=1;}
 key.generationLow=uint32_t(worldProducerGeneration);key.generationHigh=uint32_t(worldProducerGeneration>>32);
 key.connectionLow=uint32_t(worldFrameServer.connection());key.connectionHigh=uint32_t(worldFrameServer.connection()>>32);
 if(asyncCommand){
  // The worker exists to serve the live Host session. Retained page leases are
  // preserved across a disconnect, but cannot justify starting a new worker
  // while the Host heartbeat/subscription is absent.
  if(!worldFrameServer.wanted(key.owner,key.tick)){
   worldFrameReady=false;worldFrameBoundaryOnly=false;return false;
  }
 }
 worldFrameServer.observe(key);
#if !defined(WORLD_FRAME_TEST_SYNC_CAPTURE) && !defined(WORLD_FRAME_PLATFORM_FIXTURE)
 // Host VisualCache used a second independent CaptureGraph and a zero-page
 // boundary source. Both are quarantined until display data is part of the
 // same Runtime-owned immutable publication as command/automatic data.
 if(boundaryOnly){worldFrameReady=false;worldFrameBoundaryOnly=false;return false;}
#endif
 // A successful strict sample becomes a read-only candidate on the NEXT
 // simulation frame. Its hash can be reused without any 16K-game-roster scan;
 // the native admission path still requires live per-object validation.
 if(asyncCommand&&worldFrameReady&&!worldFrameBoundaryOnly&&
    worldFrame.key.sameWorld(key)&&worldFrame.key.frame!=key.frame&&
    worldFrameCommandRoster.current(worldFrame.key)){
  // Snapshot and UID roster must always travel as a single immutable
  // source pair. Moving only the roster would break provenance and cause
  // observational planners to reject every otherwise valid candidate.
  worldFrameCandidate.swap(worldFrame);
  worldFrameCandidateRoster.swap(worldFrameCommandRoster);
  worldFrameReady=false;
  worldFrameCandidateAvailable=true;
  if(!++worldFrameCandidateVersion)++worldFrameCandidateVersion;
 }
 const uint64_t requestRevision=(uint64_t(worldFrameServer.requestedRevision())<<32)|runtimeDemandRevision;
 // Host page discovery is independent from Runtime command completeness. Keep
 // a valid same-frame strict sample live while the worker refreshes Host-only
 // pages; only a new Runtime lease, graph requirement, world/frame or roster
 // configuration can invalidate it.
 const bool strictReusable=asyncCommand&&worldFrameReady&&!worldFrameBoundaryOnly&&!runtimeDemandOverflow&&
  worldframe::commandSampleReusable(worldFrame.key,key,worldFrameCapturedDemand,graphDemand,
   worldFrameCapturedRevision,requestRevision,worldFrameCommandRoster.current(worldFrame.key),
   worldFrameCommandRoster.global,globalVehicles);
 if(haveCaptureAttempt&&lastCaptureAttempt.sameFrame(key)){
  // A paused frame still needs a new worker request when a display is opened,
  // a dependency is missing, or the previous Host publication was retired.
  // Host-only revision changes refresh that publication without revoking the
  // already sealed Runtime command sample.
  const bool reuseAsync=strictReusable&&worldFrameServer.hasPublication()&&
      requestRevision==worldFrameCapturedRevision;
  if(worldFrameReady&&!(worldFrameBoundaryOnly&&!boundaryOnly)&&(!asyncCommand||reuseAsync)){
   // The async worker owns Host-only transport, including same-frame
   // republishing; never interleave the older strict publisher in its slot.
   if(!asyncCommand)worldFramePublication.publish(worldFrame,worldFrameServer);
   return true;
  }
  // A hotkey/manual order admitted in the same logic frame after a display
  // boundary snapshot must not inherit that reduced read set. This one-way
  // escalation is exceptional and publishes a new serial, never mixed bytes.
  // A failed attempt was never published. Retry transient failures even if
  // the game has paused, with bounded backoff rather than every worker wake.
  if(!asyncCommand&&uint32_t(key.tick-failedCaptureAt)<captureRetryDelay)return false;
 }else captureRetryDelay=0;
 if(asyncCommand){
  // The native callback NEVER executes CaptureGraph in this mode. An
  // incomplete/later-frame graph is *not* a license to scan all live actors.
  // The existing 4ms Host pulse gives another same-frame opportunity to
  // adopt a worker result without blocking the game's message pump.
  // The command worker already publishes complete *read-only* world graphs
  // to Host IPC. Keep native exact-frame acceptance separate and avoid a
  // second publisher racing to overwrite the Host observation channel.
  if(worldFrameCommandWorker.take(key,graphDemand,requestRevision,worldFrame,
                                  &worldFrameCommandRoster,globalVehicles)){
   // A background success still needs a NEW independent synchronous
   // world/frame/House identity check immediately before command delivery.
   const auto after=worldlife::capture(live);uint32_t afterHouse=0;
   const bool stable=live(0xA83D4C,&afterHouse,4)&&after.readable&&after.valid()&&
       before.same(after)&&before.frame==after.frame&&house==afterHouse&&
       worldFrame.key.sameFrame(key)&&worldFrame.sealed()&&!worldFrame.overflow();
   if(stable){
    key.serial=++worldFrameSerial;
    worldFrame.key.serial=key.serial;
    worldFrame.key.demand=uint32_t(requestRevision>>32)&~worldframe::PublicationKeyFlags;
    lastCaptureAttempt=key;haveCaptureAttempt=true;captureRetryDelay=0;
    runtimeWorldDemands.clear();runtimeDemandBits.fill(0);
    if(!++runtimeDemandRevision)++runtimeDemandRevision;
    worldFrameCapturedDemand=graphDemand;
    worldFrameCapturedRevision=(requestRevision&0xffffffff00000000ull)|runtimeDemandRevision;
    worldFrameBoundaryOnly=false;worldFrameReady=true;
    // The authoritative native sample stays local; Host receives only
    // worker-tagged observation-only publications, never strict EventClass
    // permission. Mixing both writers caused stale/oscillating Host serials.
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
    if(sh)++sh->sampling.strictAdopted;
#endif
    return true;
   }
   worldFrameServer.retire();worldFrameCommandWorker.invalidate();
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
   if(sh)++sh->sampling.strictWorldRejected;
#endif
   if(sh)++sh->worldSampleFailures;
  }
  // A worker may have completed after the simulation frame advanced.
  // Keep its *complete* UID graph as a bounded discovery candidate without
  // marking it as a current command sample or publishing mixed-frame bytes.
  if(worldFrameCommandWorker.takeObservation(key,worldframe::Base,worldFrameCandidate,
                                             worldFrameCandidateRoster,globalVehicles)){
   worldFrameCandidateAvailable=true;
   if(!++worldFrameCandidateVersion)++worldFrameCandidateVersion;
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
   if(sh)++sh->sampling.candidateAdopted;
#endif
  }
  // A candidate refresh may arrive while a still-current strict Runtime
  // sample is in use. Never discard that sample merely because Host-only
  // page demand changed; candidate bytes remain observation-only.
  if(!strictReusable){worldFrameReady=false;worldFrameBoundaryOnly=false;}
  // Always poll take() first. An identical pending request skips expensive
  // page-list assembly, but paused-frame recovery retries after 16ms.
  if(commandRequestGate.defer(key,graphDemand,requestRevision,globalVehicles,key.tick)){
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
   if(sh)++sh->sampling.gateDeferred;
#endif
   return strictReusable;
  }
  // A matching in-flight worker already owns this request; avoid rebuilding
  // even the 16K-page dependency list on the Game callback.
  // take() above still rejects every stale / previous-frame result.
  if(worldFrameCommandWorker.deferBusy(key,graphDemand,requestRevision,globalVehicles)){
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
   if(sh)++sh->sampling.workerDeferred;
#endif
   return strictReusable;
  }
  // Only a deduplicated, bounded address list is assembled on the game
  // thread. Overflow invalidates the whole request; the vector is ownership-
  // swapped to the worker. Sorting, page I/O and graph traversal stay off-thread.
  if(runtimeDemandOverflow){
   // The per-feature demand set exceeded the single-sample page budget.
   // Preserve leases, drop only this oversized transient list, and retry
   // after the active demand set changes. Never submit a truncated graph.
   runtimeDemandOverflow=false;runtimeWorldDemands.clear();runtimeDemandBits.fill(0);
   worldFrameCaptureRequests.clear();
   if(!++runtimeDemandRevision)++runtimeDemandRevision;
   ++sh->worldSampleFailures;return false;
  }
  auto&requests=worldFrameCaptureRequests;
  // The worker returns its previous vector through the queue slot when it
  // dequeues a newer job. Reclaim it before building, never after submission.
  worldFrameCommandWorker.reclaimRequestBuffer(requests);
  const auto&hostRequests=worldFrameServer.requests();
  if(!worldFrameRequestBufferPrepared){
   requests.reserve(worldframe::MaxPages);
   worldFrameRequestBufferPrepared=true;
  }
  requests.clear();worldFrameRequestBits.fill(0);
  bool requestOverflow=false;
  auto appendPage=[&](uint32_t page)->bool{
   if((page&4095u)||!worldframe::range(page,4096)){requestOverflow=true;return false;}
   const uint32_t index=page>>12;
   if((index>>6)>=worldFrameRequestBits.size()){requestOverflow=true;return false;}
   const uint64_t mask=uint64_t(1)<<(index&63);
   if(worldFrameRequestBits[index>>6]&mask)return true;
   if(requests.size()>=worldframe::MaxPages){requestOverflow=true;return false;}
   worldFrameRequestBits[index>>6]|=mask;requests.push_back(page);return true;
  };
  bool complete=true;
  for(uint32_t page:hostRequests)if(!appendPage(page)){complete=false;break;}
  if(complete)for(uint32_t page:runtimeWorldDemands)if(!appendPage(page)){complete=false;break;}
  if(complete)complete=runtimeDemandLeases.visit(key.tick,appendPage);
  if(!complete||requestOverflow){
   // Host-only demand overflow must not revoke an already complete Runtime
   // command sample. If Runtime demand changed, strictReusable is false and
   // the old sample is retired before retrying the bounded request list.
   if(!strictReusable){
    runtimeWorldDemands.clear();runtimeDemandBits.fill(0);
    if(!++runtimeDemandRevision)++runtimeDemandRevision;
   }
   requests.clear();++sh->worldSampleFailures;return strictReusable;
  }
  // Server::requests() may publish a newer Host demandRevision than the
  // version sampled before assembling the request list. Stamp the exact
  // version that supplied these pages, not the obsolete pre-read version.
  // take() will still reject the result if the demand changes again.
  const uint64_t assembledRevision=
      (uint64_t(worldFrameServer.demandRevision())<<32)|runtimeDemandRevision;
  if(worldFrameCommandWorker.request(key,graphDemand,assembledRevision,requests,globalVehicles)){
   if(graphDemand&worldframe::TypeCatalogue)worldFrameCatalogueProbeAt=key.tick;
   commandRequestGate.submitted(key,graphDemand,assembledRevision,globalVehicles);
#if !defined(WORLD_FRAME_PLATFORM_FIXTURE) && !defined(WORLD_FRAME_FIXTURE_ONLY)
   if(sh)++sh->sampling.requestsQueued;
#endif
  }
  return strictReusable;
 }
#if defined(WORLD_FRAME_TEST_SYNC_CAPTURE) || defined(WORLD_FRAME_PLATFORM_FIXTURE)
 if(!worldFramePublication.begin(worldFrame))return false;
 lastCaptureAttempt=key;haveCaptureAttempt=true;worldFrameReady=false;
 // A Host-only display acquisition never consumes or sorts the command
 // dependency queue. The Host VisualCache owns its own page discovery, and
 // a later same-frame hotkey must still see every pending command lease.
 auto&requests=worldFrameCaptureRequests;
 if(boundaryOnly)requests.clear(); // Do not even EVALUATE Server::requests().
 else assist::capturepolicy::prepareRequestedPages(false,key.tick,
  worldFrameServer.requests(),runtimeWorldDemands,runtimeDemandLeases,requests,
  &worldFrameCaptureScratch.warmSortScratch);
 key.serial=++worldFrameSerial;
 key.demand=(worldFrameServer.demandRevision()&~worldframe::BoundaryOnlyKeyBit)|
            (boundaryOnly?worldframe::BoundaryOnlyKeyBit:0u);
 if(boundaryOnly)worldFrame.resetBoundary(key);else worldFrame.reset(key);
 if(boundaryOnly){
  // Truly zero-page Host display boundary. The independent Host VisualCache
  // acquires its own image, unit/Type graph and map geometry. No CaptureGraph,
  // page discovery, page requests, UID scan or native EventClass authority
  // belongs in this display-only game callback.
  // Publication is still gated by TWO independent live world/House reads;
  // a mixed or changed game frame is never advertised as a display source.
  const auto after=worldlife::capture(live);uint32_t afterHouse=0;
  const bool stable=live(0xA83D4C,&afterHouse,4)&&after.readable&&after.valid()&&
    before.same(after)&&before.frame==after.frame&&house==afterHouse;
  if(!stable||!worldFrame.seal()){
   worldFrameBoundaryOnly=false;
   if(!stable)worldFrameServer.retire();
   failedCaptureAt=key.tick;
   captureRetryDelay=captureRetryDelay?std::min<uint32_t>(250,captureRetryDelay*2):16;
   if(sh)++sh->worldSampleFailures;
   return false;
  }
  captureRetryDelay=0;worldFrameBoundaryOnly=true;worldFrameReady=true;
  // A later native command in this same logic frame must bypass the reuse
  // path above and build a separate fully sealed sample with a new serial.
  worldFramePublication.publish(worldFrame,worldFrameServer);
  if(sh){
   InterlockedIncrement(&sh->worldSampleSeq);sh->worldSampleFrame=key.frame;
   sh->worldSamplePages=0;sh->worldSampleReads=0;sh->worldSampleBytes=0;
   sh->worldSampleRosters=0;sh->worldSampleSerial=key.serial;
   MemoryBarrier();InterlockedIncrement(&sh->worldSampleSeq);
  }
  return true;
 }
 // Probe up to 64 fields on the worker but permit only 16 freshly
 // verified fields to enter this logic frame. Both arrays are stack-bounded.
 // Rotate the direct-mapped hint table so busy units are not permanently first.
 std::array<worldframe::RequestedSpan,worldframe::RecoverySpans>candidates{};
 std::array<worldframe::RequestedSpan,worldframe::RecoveryConfirmSpans>verified{};
 size_t candidateCount=0,verifiedCount=0;
 const size_t start=key.serial%runtimeMissingSpans.size();
 for(size_t step=0;!boundaryOnly&&step<runtimeMissingSpans.size()&&candidateCount<candidates.size();++step){
  const auto&f=runtimeMissingSpans[(start+step)%runtimeMissingSpans.size()];
  if(f.bytes&&f.bytes<=256&&worldframe::range(f.address,f.bytes)&&
     uint32_t(key.tick-f.tick)<=1000)candidates[candidateCount++]=f;
 }
 // Background bytes are NEVER reused as same-frame command observations.
 // Successful probes only choose which exact ranges the original producer
 // reads under its existing before/after identity and frame validation.
 std::array<worldframe::RequestedSpan,worldframe::RecoverySpans>background{};
 const size_t readyCount=worldFramePublication.recovered(key,background.data(),background.size());
 for(size_t i=0;i<readyCount&&verifiedCount<verified.size();++i)
  for(size_t j=0;j<candidateCount;++j)
   if(background[i].address==candidates[j].address&&background[i].bytes==candidates[j].bytes){
    verified[verifiedCount++]=candidates[j];break;
   }
 // Do not turn a cold worker start or a dropped latest-work slot into total
 // starvation. Two synchronous field retries are the strict fallback limit.
 if(!verifiedCount)for(size_t i=0;i<std::min<size_t>(2,candidateCount);++i)verified[verifiedCount++]=candidates[i];
 if(candidateCount&&worldFrameServer.wanted(key.owner,key.tick)&&
    (!lastRecoverySubmitAt||uint32_t(key.tick-lastRecoverySubmitAt)>=32)){
  if(worldFramePublication.requestRecovery(key,candidates.data(),candidateCount))lastRecoverySubmitAt=key.tick;
 }
 worldframe::CaptureGraph<Live>capture(worldFrame,live,worldFrameCaptureScratch);
 bool complete=capture.capture(captureDemand,requests,verified.data(),verifiedCount);
 // These checks deliberately bypass the read set. Cached self-comparison is
 // not evidence that the frame/world/House survived the acquisition boundary.
 const auto after=worldlife::capture(live);uint32_t afterHouse=0;
 const bool stable=live(0xA83D4C,&afterHouse,4)&&after.readable&&after.valid()&&before.same(after)&&before.frame==after.frame&&house==afterHouse;
 if(!complete||!stable||!worldFrame.seal()){
  worldFrameBoundaryOnly=false;
  // A failed roster/page acquisition does not revoke the last complete Host
  // publication. Its original age/identity still decide whether it is usable.
  // World/House changes invalidate it immediately, independent of graph failure.
  if(!stable)worldFrameServer.retire();failedCaptureAt=key.tick;captureRetryDelay=captureRetryDelay?std::min<uint32_t>(250,captureRetryDelay*2):16;
  if(sh)++sh->worldSampleFailures;return false;
 }
 captureRetryDelay=0;
 if(graphDemand&worldframe::TypeCatalogue){
  worldFrameCatalogueProbeAt=key.tick;
  worldframe::stampTypeCatalogue(worldFrame); // synchronous test fixture only
 }
 // Remove confirmed exact misses from the hint ring. Otherwise an old
 // successful worker result would cause extra live reads on every frame.
 for(size_t i=0;i<verifiedCount;++i){
  uint8_t bytes[256]{};
  const auto&f=verified[i];
  if(!worldFrame.read(f.address,bytes,f.bytes))continue;
  for(auto&hint:runtimeMissingSpans)
   if(hint.address==f.address&&hint.bytes==f.bytes){hint={};break;}
 }
 // A display boundary is not a command sample. Do not lose command ranges
 // that would be required by a later same-frame escalation.
 if(!boundaryOnly){runtimeWorldDemands.clear();runtimeDemandBits.fill(0);}
 worldFrameBoundaryOnly=boundaryOnly;
 worldFrameReady=true;worldFramePublication.publish(worldFrame,worldFrameServer);
 if(sh){InterlockedIncrement(&sh->worldSampleSeq);sh->worldSampleFrame=key.frame;sh->worldSamplePages=uint32_t(worldFrame.pages.size());sh->worldSampleReads=worldFrame.counters.calls;sh->worldSampleBytes=worldFrame.counters.bytes;sh->worldSampleRosters=worldFrame.counters.rosterCaptures;sh->worldSampleSerial=key.serial;MemoryBarrier();InterlockedIncrement(&sh->worldSampleSeq);}
 return true;
#else
 // No production synchronous full-graph path exists. If the worker did not
 // adopt or queue a sample, consumers remain unavailable until it can.
 worldFrameReady=false;worldFrameBoundaryOnly=false;
 return false;
#endif
}
