#pragma once
#include "../../shared/source/world_frame_capture.h"
#include "../../shared/source/world_frame_transport.h"
#include "world_capture_requests.h"

static worldframe::Snapshot worldFrame;
static worldframe::Server worldFrameServer;
#ifdef WORLD_FRAME_PLATFORM_FIXTURE
// Existing synchronous transport fixtures keep their explicit execution seam.
struct WorldFramePublication {bool begin(worldframe::Snapshot&){return true;}void invalidate(){}bool stop(){return true;}bool publish(const worldframe::Snapshot&frame,const worldframe::Server&){return worldFrameServer.publish(frame);}};
#else
#include "../../shared/source/world_frame_async_win.h"
using WorldFramePublication=worldframe::AsyncPublisher<>;
#endif
static WorldFramePublication worldFramePublication;
static uint32_t worldFrameSerial=0;static unsigned sampleReadDepth=0;static bool sampleReadComplete=true;
static std::array<uint64_t,8192>runtimeDemandBits{};static std::vector<uint32_t>runtimeWorldDemands;
static worldframe::DemandLeases runtimeDemandLeases;
static worldframe::CaptureScratch worldFrameCaptureScratch{};static std::vector<uint32_t>worldFrameCaptureRequests;
static bool worldFrameReady=false;static bool worldFrameBoundaryOnly=false;
static worldframe::Key lastCaptureAttempt{};static bool haveCaptureAttempt=false;
static uint32_t failedCaptureAt=0,captureRetryDelay=0;
static uint64_t worldProducerGeneration=0;
static void requestWorldRange(uint32_t at,size_t n){
 if(!worldframe::range(at,n))return;
 for(uint32_t page=at&~4095u;uint64_t(page)<uint64_t(at)+n;page+=4096){const uint32_t index=page>>12;const uint64_t mask=uint64_t(1)<<(index&63);
  if(!(runtimeDemandBits[index>>6]&mask)){runtimeDemandBits[index>>6]|=mask;runtimeWorldDemands.push_back(page);runtimeDemandLeases.retain(page,worldFrame.key.tick);}}
}
static bool readWorldSample(uint32_t at,void*out,size_t n){
 requestWorldRange(at,n); // Keep successful consumer dependencies subscribed.
 const bool ok=worldFrameReady&&worldFrame.read(at,out,n);
 if(!ok)sampleReadComplete=false;return ok;
}
class SampleReadScope {
 unsigned previousDepth_;bool previousComplete_,ready_,finished_=false,result_=false;
public:
 SampleReadScope():previousDepth_(sampleReadDepth),previousComplete_(sampleReadComplete),ready_(worldFrameReady&&!worldFrameBoundaryOnly&&worldFrame.sealed()&&worldFrame.key.match==match&&worldFrame.key.frame==snap.frame){++sampleReadDepth;sampleReadComplete=ready_;}
 explicit operator bool()const{return ready_;}
 bool complete()const{return finished_?result_:ready_&&sampleReadComplete;}
 void finish(){if(finished_)return;result_=complete();sampleReadDepth=previousDepth_;sampleReadComplete=previousComplete_&&(!previousDepth_||result_);finished_=true;}
 ~SampleReadScope(){finish();}
 SampleReadScope(const SampleReadScope&)=delete;SampleReadScope&operator=(const SampleReadScope&)=delete;
};
static void retireWorldFrame(){worldFramePublication.invalidate();worldFrameBoundaryOnly=false;if(worldProducerGeneration&&++worldProducerGeneration==0)++worldProducerGeneration;worldFrameReady=false;haveCaptureAttempt=false;captureRetryDelay=0;worldFrameServer.retire();runtimeWorldDemands.clear();runtimeDemandLeases.clear();worldFrameCaptureRequests.clear();runtimeDemandBits.fill(0);}
template<class Live>static bool captureWorldFrame(Live live,const worldlife::Sample&before,uint32_t house,DWORD now,uint32_t demand,bool permitDisplayBoundary=false){
 // Only an explicitly admitted all-display subscription may skip the
 // game-thread CaptureGraph. Full command sampling has distinct authority.
 const bool boundaryOnly=permitDisplayBoundary&&demand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly);
 worldframe::Key key{GetCurrentProcessId(),sh?sh->owner:0,match,0,GetTickCount(),0,before.scenario,before.cells,house,uint32_t(before.count),before.frame,before.session};
 if(!worldProducerGeneration){LARGE_INTEGER stamp{};QueryPerformanceCounter(&stamp);worldProducerGeneration=uint64_t(stamp.QuadPart);if(!worldProducerGeneration)worldProducerGeneration=1;}
 key.generationLow=uint32_t(worldProducerGeneration);key.generationHigh=uint32_t(worldProducerGeneration>>32);
 key.connectionLow=uint32_t(worldFrameServer.connection());key.connectionHigh=uint32_t(worldFrameServer.connection()>>32);
 worldFrameServer.observe(key);
 if(haveCaptureAttempt&&lastCaptureAttempt.sameFrame(key)){
  // A consumer can resume while the game is paused, after its old transport
  // was retired or local automatic work advanced without a live Host reader.
  // Re-export the existing immutable sample; publish() deduplicates its exact
  // descriptor. Never recapture or invent a newer observation/acquisition tick.
  if(worldFrameReady&&!(worldFrameBoundaryOnly&&!boundaryOnly)){
   worldFramePublication.publish(worldFrame,worldFrameServer);return true;
  }
  // A command in this very frame upgrades the zero-page display token to a
  // new full-graph serial, not a mixed or stale read of the old marker.
  // A failed attempt was never published. Retry transient failures even if
  // the game has paused, with bounded backoff rather than every worker wake.
  if(uint32_t(key.tick-failedCaptureAt)<captureRetryDelay)return false;
 }else captureRetryDelay=0;
 if(!worldFramePublication.begin(worldFrame))return false;
 lastCaptureAttempt=key;haveCaptureAttempt=true;worldFrameReady=false;
 auto&requests=worldFrameCaptureRequests;
 if(boundaryOnly){
  // Do not even call Server::requests() on the game thread: Host display
  // page queues may be huge and belong to the Host's own VisualCache.
  requests.clear();
 }else{
  capturepolicy::prepareRequestedPages(false,key.tick,worldFrameServer.requests(),
                                       runtimeWorldDemands,runtimeDemandLeases,requests);
 }
 key.serial=++worldFrameSerial;
 key.demand=(worldFrameServer.demandRevision()&~worldframe::BoundaryOnlyKeyBit)|
           (boundaryOnly?worldframe::BoundaryOnlyKeyBit:0u);
 worldFrame.reset(key);
 // Metadata-only publication takes ZERO graph/page reads in the game tick.
 bool complete=true;
 if(!boundaryOnly){
  worldframe::CaptureGraph<Live>capture(worldFrame,live,worldFrameCaptureScratch);
  complete=capture.capture(demand&~uint32_t(worldframe::HostDisplayOnly),requests);
 }
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
 // Do not acknowledge command page leases from a display-only boundary.
 if(!boundaryOnly){runtimeWorldDemands.clear();runtimeDemandBits.fill(0);}
 worldFrameBoundaryOnly=boundaryOnly;
 worldFrameReady=true;worldFramePublication.publish(worldFrame,worldFrameServer);
 if(sh){InterlockedIncrement(&sh->worldSampleSeq);sh->worldSampleFrame=key.frame;sh->worldSamplePages=uint32_t(worldFrame.pages.size());sh->worldSampleReads=worldFrame.counters.calls;sh->worldSampleBytes=worldFrame.counters.bytes;sh->worldSampleRosters=worldFrame.counters.rosterCaptures;sh->worldSampleSerial=key.serial;MemoryBarrier();InterlockedIncrement(&sh->worldSampleSeq);}
 return true;
}
