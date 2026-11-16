#pragma once
#include "world_frame_capture.h"

// First phase of moving native memory reads off the game thread: an isolated
// shadow acquisition for measuring real graph cost and detecting mixed worlds.
// No Snapshot/token escapes this API. A candidate may NEVER be used for native
// orders, nor silently replace an authoritative game-thread sample.
//
// An unchanged frame counter is only necessary, NOT proof of atomicity:
// mutations by hooks/input/other threads can occur without frame increments.
// Command cutover needs a separate authoritative mutation/read barrier.
namespace worldframe {
enum class BackgroundTrialStatus:uint8_t {
 InvalidBoundary, CaptureFailure, WorldChanged, CandidateOnly
};
struct BackgroundTrialResult {
 BackgroundTrialStatus status=BackgroundTrialStatus::InvalidBoundary;
 Key before{},after{};
 Counters counters{};
 uint32_t pages=0;
 // Includes the independent before/after boundary reads, not merely graph
 // reads. The caller owns the wall-clock duration and actual read thread.
 uint32_t frameChecks=0;
 bool usableForCommands() const {return false;}
};

template<class Stamp,class Read>
BackgroundTrialResult backgroundTrial(Stamp stamp,Read read,uint32_t demand,
                                       const std::vector<uint32_t>&requests={}){
 BackgroundTrialResult out{};
 out.before=stamp();++out.frameChecks;
 if(out.before.pid==0||out.before.house<0x10000||out.before.scenario<0x10000||
    out.before.cells<0x10000||out.before.match==0||out.before.frame<0)return out;
 // These allocations and engine graph reads happen on the *calling thread*.
 // Production may schedule this function on a DLL-pinned background worker.
 Snapshot sample;
 sample.reset(out.before);
 CaptureScratch scratch;
 CaptureGraph<Read> graph(sample,read,scratch);
 const bool complete=graph.capture(demand,requests);
 const bool sealed=sample.seal();
 out.counters=sample.counters;out.pages=uint32_t(sample.pages.size());
 out.after=stamp();++out.frameChecks;
 if(!out.before.sameFrame(out.after)){
  out.status=BackgroundTrialStatus::WorldChanged;return out;
 }
 out.status=complete&&sealed&&!sample.overflow()?
   BackgroundTrialStatus::CandidateOnly:BackgroundTrialStatus::CaptureFailure;
 return out;
}
} // namespace worldframe
