#pragma once
#include <cstdint>
namespace assist { namespace burstcompute {
constexpr int MaxSources=64;
// The host and Runtime run this same value-only state decision. Observation
// and accepted transitions stay in Runtime; no timer or burst-index writes.
struct Source {
 uint32_t job=0,id=0,pointer=0,token=0,target=0,targetPointer=0,weapon=0;
 int phase=0,frame=0,sentFrame=0,mission=0,queued=-1,burst=0,index=0,fireStamp=0,previousFire=0;
 uint32_t currentTarget=0,eligible=0,inRange=0,moving=0;
};
struct Decision {int mission=-1;uint32_t done=0;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0;Source sources[MaxSources];};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Decision decisions[MaxSources];};
inline Decision decide(const Source&s){
 Decision d;if(s.moving){d.done=1;return d;}
 if(!s.phase){
  if(s.eligible&&s.inRange&&s.currentTarget==s.target&&s.mission==1&&(s.queued==-1||s.queued==1)&&s.burst>=2&&s.index>0&&s.index<s.burst)d.mission=13;
  return d;
 }
 if(s.phase==1){
  if(s.currentTarget&&s.currentTarget!=s.target){d.done=1;return d;}
  bool stopped=!s.currentTarget&&(s.mission==13||s.mission==5||s.mission==6);
  // If the stop state was too brief to observe, enqueue the one restoration
  // after the existing Stop, preserving event order. Never repeat Stop here.
  if(s.frame>s.sentFrame&&(stopped||s.frame-s.sentFrame>=90)){
   if(s.inRange)d.mission=1;else d.done=1;
  }return d;
 }
 if(s.currentTarget&&s.currentTarget!=s.target){d.done=1;return d;}
 if(s.currentTarget==s.target&&s.mission==1&&s.index>0&&s.fireStamp!=s.previousFire)d.done=1;
 if(s.frame-s.sentFrame>=90)d.done=1;
 return d;
}
inline Result calculate(const Input&i){Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;
 if(i.count<0||i.count>MaxSources)return r;r.count=i.count;for(int n=0;n<i.count;++n)r.decisions[n]=decide(i.sources[n]);return r;}
}}
