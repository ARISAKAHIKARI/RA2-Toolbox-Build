#pragma once
#include <stdint.h>
#include <stddef.h>
#include <type_traits>

namespace assist { namespace hardwareperf {
inline constexpr uint32_t Version=1,IntervalMs=1000;
// Cumulative owner-thread operation counters, not timings. The feature
// counters count attempted callbacks; `changed` counts all successful feature
// callbacks, including the single-player FPS service. It is not control-only.
struct Counters {
 uint32_t exceptions=0,changed=0,control=0,full=0,enhancement=0,map=0;
 uint32_t admissionChecks=0,worldCaptures=0;
};
inline Counters difference(const Counters&a,const Counters&b){
 return {a.exceptions-b.exceptions,a.changed-b.changed,a.control-b.control,
  a.full-b.full,a.enhancement-b.enhancement,a.map-b.map,
  a.admissionChecks-b.admissionChecks,a.worldCaptures-b.worldCaptures};
}
struct Snapshot {
 uint32_t version=Version,serial=0,match=0,tick=0,intervalMs=0;
 int32_t firstLogicFrame=-1,lastLogicFrame=-1;
 uint32_t observedLogicFrames=0;
 Counters cumulative{},delta{};
};
static_assert(std::is_standard_layout<Snapshot>::value&&std::is_trivially_copyable<Snapshot>::value,"hardware performance payload is a value type");
static_assert(sizeof(Counters)==32&&sizeof(Snapshot)==96&&offsetof(Snapshot,cumulative)==32,"hardware performance payload must match x86/x64");

// Called only from ordinary owner maintenance. Frame values describe the
// maintenance observations, not a per-hit frame attribution or FPS counter.
// An intervalMs==0 snapshot starts a new world/window and has no rate yet.
class Window {
 bool initialized_=false;uint32_t match_=0,startTick_=0,serial_=0;
 int32_t firstFrame_=-1,lastFrame_=-1;uint32_t observedFrames_=0;
 Counters start_{};
 uint32_t serial(){if(!++serial_)++serial_;return serial_;}
 void start(uint32_t now,uint32_t match,int32_t frame,const Counters&totals){
  initialized_=true;match_=match;startTick_=now;firstFrame_=lastFrame_=frame;
  observedFrames_=frame>=0?1u:0u;start_=totals;
 }
public:
 bool observe(uint32_t now,uint32_t match,int32_t frame,const Counters&totals,Snapshot&out){
  // A rewind or world transition must not combine unrelated sessions. Tick
  // and diagnostic counters deliberately use unsigned wrap-safe subtraction.
  if(!initialized_||match!=match_||frame<lastFrame_){
   start(now,match,frame,totals);out={Version,serial(),match,now,0,frame,frame,observedFrames_,totals,{}};return true;
  }
  if(frame!=lastFrame_){if(frame>=0)++observedFrames_;lastFrame_=frame;}
  const uint32_t elapsed=now-startTick_;if(elapsed<IntervalMs)return false;
  out={Version,serial(),match,now,elapsed,firstFrame_,frame,observedFrames_,totals,difference(totals,start_)};
  start(now,match,frame,totals);return true;
 }
};
}}
