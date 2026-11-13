#pragma once
#include "../../shared/source/world_frame_capture.h"
#include "../../shared/source/world_frame_transport.h"

static worldframe::Snapshot worldFrame;
static worldframe::Server worldFrameServer;
static uint32_t worldFrameSerial=0;static unsigned sampleReadDepth=0;static bool sampleReadComplete=true;
static std::array<uint64_t,8192>runtimeDemandBits{};static std::vector<uint32_t>runtimeWorldDemands;
static worldframe::DemandLeases runtimeDemandLeases;
static worldframe::CaptureScratch worldFrameCaptureScratch{};static std::vector<uint32_t>worldFrameCaptureRequests;
static bool worldFrameReady=false;static worldframe::Key lastCaptureAttempt{};static bool haveCaptureAttempt=false;
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
 SampleReadScope():previousDepth_(sampleReadDepth),previousComplete_(sampleReadComplete),ready_(worldFrameReady&&worldFrame.sealed()&&worldFrame.key.match==match&&worldFrame.key.frame==snap.frame){++sampleReadDepth;sampleReadComplete=ready_;}
 explicit operator bool()const{return ready_;}
 bool complete()const{return finished_?result_:ready_&&sampleReadComplete;}
 void finish(){if(finished_)return;result_=complete();sampleReadDepth=previousDepth_;sampleReadComplete=previousComplete_&&(!previousDepth_||result_);finished_=true;}
 ~SampleReadScope(){finish();}
 SampleReadScope(const SampleReadScope&)=delete;SampleReadScope&operator=(const SampleReadScope&)=delete;
};
static void retireWorldFrame(){worldFrameReady=false;haveCaptureAttempt=false;captureRetryDelay=0;worldFrameServer.retire();runtimeWorldDemands.clear();runtimeDemandLeases.clear();worldFrameCaptureRequests.clear();runtimeDemandBits.fill(0);}
template<class Live>static bool captureWorldFrame(Live live,const worldlife::Sample&before,uint32_t house,DWORD now,uint32_t demand){
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
  if(worldFrameReady){worldFrameServer.publish(worldFrame);return true;}
  // A failed attempt was never published. Retry transient failures even if
  // the game has paused, with bounded backoff rather than every worker wake.
  if(uint32_t(key.tick-failedCaptureAt)<captureRetryDelay)return false;
 }else captureRetryDelay=0;
 lastCaptureAttempt=key;haveCaptureAttempt=true;worldFrameReady=false;
 auto&requests=worldFrameCaptureRequests;const auto&hostRequests=worldFrameServer.requests();requests.assign(hostRequests.begin(),hostRequests.end());requests.insert(requests.end(),runtimeWorldDemands.begin(),runtimeWorldDemands.end());
 // Pending consumption can predate a long pause or failed capture. Renew its
 // addresses at this acquisition boundary and acknowledge only on success.
 for(uint32_t page:runtimeWorldDemands)runtimeDemandLeases.retain(page,key.tick);
 runtimeDemandLeases.append(key.tick,requests);
 std::sort(requests.begin(),requests.end());requests.erase(std::unique(requests.begin(),requests.end()),requests.end());
 key.serial=++worldFrameSerial;key.demand=worldFrameServer.demandRevision();worldFrame.reset(key);
 worldframe::CaptureGraph<Live>capture(worldFrame,live,worldFrameCaptureScratch);bool complete=capture.capture(demand,requests);
 // These checks deliberately bypass the read set. Cached self-comparison is
 // not evidence that the frame/world/House survived the acquisition boundary.
 const auto after=worldlife::capture(live);uint32_t afterHouse=0;
 const bool stable=live(0xA83D4C,&afterHouse,4)&&after.readable&&after.valid()&&before.same(after)&&before.frame==after.frame&&house==afterHouse;
 if(!complete||!stable||!worldFrame.seal()){
  // A failed roster/page acquisition does not revoke the last complete Host
  // publication. Its original age/identity still decide whether it is usable.
  // World/House changes invalidate it immediately, independent of graph failure.
  if(!stable)worldFrameServer.retire();failedCaptureAt=key.tick;captureRetryDelay=captureRetryDelay?std::min<uint32_t>(250,captureRetryDelay*2):16;
  if(sh)++sh->worldSampleFailures;return false;
 }
 captureRetryDelay=0;
 runtimeWorldDemands.clear();runtimeDemandBits.fill(0);
 worldFrameReady=true;worldFrameServer.publish(worldFrame);
 if(sh){InterlockedIncrement(&sh->worldSampleSeq);sh->worldSampleFrame=key.frame;sh->worldSamplePages=uint32_t(worldFrame.pages.size());sh->worldSampleReads=worldFrame.counters.calls;sh->worldSampleBytes=worldFrame.counters.bytes;sh->worldSampleRosters=worldFrame.counters.rosterCaptures;sh->worldSampleSerial=key.serial;MemoryBarrier();InterlockedIncrement(&sh->worldSampleSeq);}
 return true;
}
