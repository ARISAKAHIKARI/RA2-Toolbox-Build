#pragma once
#include <cstdint>
#include <climits>
#include "unload_policy.h"
namespace assist { namespace unloadcompute {
// Plain observations only. No process addresses are dereferenced by this code.
constexpr int MaxSources=256,MaxTargets=16384;
struct State {
 int stage=0,sentFrame=0,seenUnload=0,retries=0,x=0,y=0,stableSince=0,deployedSince=0;
 uint32_t sleepObserved=0;
};
struct View {
 int frame=0,mode=0,landedFrame=0,mission=0,queued=0,sequence=0,x=0,y=0;
 uint32_t deployer=0,ready=0,engineer=0,spy=0,c4=0,queryFailed=0;
};
struct Source {uint32_t job=0,id=0,pointer=0,type=0,token=0;State state;View view;};
struct Target {uint32_t id=0,pointer=0;int x=0,y=0;};
struct Decision {
 State observed,accepted;
 int mission=-1;uint32_t target=0,done=0,doneAfter=0,follow=0;
};
struct Input {
 uint32_t match=0,tick=0;int frame=-1,count=0,targetCount=0;
 Source sources[MaxSources];Target targets[MaxTargets];
 // 0=not legal; otherwise (mission + 1) | 64 for a specialist building action.
 uint8_t actions[MaxSources][MaxTargets]{};
};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Decision decisions[MaxSources];};
inline bool valid(const Input&i){return i.count>=0&&i.count<=MaxSources&&i.targetCount>=0&&i.targetCount<=MaxTargets;}
inline Decision decide(const Source&s,const Target*targets,const uint8_t*actions,int count){
 Decision d;d.observed=s.state;auto&r=d.observed;const auto&v=s.view;const int frame=v.frame;
 auto send=[&](int mission,bool done=false){d.mission=mission;d.doneAfter=done;d.accepted=r;};
 const bool ejectDeployer=v.deployer&&v.mode!=1;
 if(v.deployer){
  if(v.x!=r.x||v.y!=r.y){r.x=v.x;r.y=v.y;r.stableSince=frame;r.deployedSince=0;}
  if(unloadpolicy::completedDeploy(v.sequence)&&v.mission!=2&&v.queued!=2){
   if(!r.deployedSince)r.deployedSince=frame;
   if(frame-r.deployedSince>=2){d.done=1;return d;}
  }else r.deployedSince=0;
  if(v.mission==16||unloadpolicy::deployObserved(v.sequence))r.seenUnload=1;
  if(unloadpolicy::deployObserved(v.sequence)||v.sequence==31||v.queued==16)return d;
  if(!r.stage){
   if(ejectDeployer&&!v.ready)return d;
   send(v.ready?16:0);d.accepted.stage=v.ready?2:1;d.accepted.sentFrame=frame;
   d.accepted.stableSince=frame;d.accepted.sleepObserved=0;d.accepted.seenUnload=0;
  }else if(r.stage==1&&(v.mission==0||v.mission==5||v.mission==11)&&frame-r.sentFrame>=2&&frame-r.stableSince>=2&&v.ready){
   send(16);d.accepted.stage=2;d.accepted.sentFrame=frame;d.accepted.seenUnload=0;
  }else if(r.stage==2&&v.mission!=16&&frame-r.sentFrame>=(r.seenUnload?12:301)){
   // A queued order can disappear without a sampled Deploy mission (blocked
   // exit or a very short interrupted animation). Wait out the native receipt
   // window before retrying an unobserved order, especially in multiplayer.
   if(ejectDeployer&&(v.mission==2||frame-r.stableSince<2))return d;
   if(v.mission!=0&&v.mission!=5&&v.mission!=6&&v.mission!=11)d.done=1;
   else if(++r.retries<(ejectDeployer?12:3)){r.stage=0;r.seenUnload=0;}
   else if(v.mission!=0)d.done=1;
   else send(5,true);
  }
  return d;
 }
 if(v.x!=r.x||v.y!=r.y||!r.stableSince){r.x=v.x;r.y=v.y;r.stableSince=frame;}
 const bool specialist=v.engineer||v.spy;
 if(v.sequence==31||(!specialist&&(frame-r.stableSince<2||v.mission==2||v.queued==2)))return d;
 if(frame<r.sentFrame)return d;
 if(v.queryFailed){r.sentFrame=frame+6;return d;}
 int best=-1;int64_t distance=INT64_MAX;bool special=false;
 for(int i=0;i<count;++i){const uint8_t action=actions[i];if(!action)continue;
  const bool candidateSpecial=(action&64)!=0;
  if(!candidateSpecial&&(special||v.engineer||v.spy))continue;
  const auto&t=targets[i];if(!t.id)continue;
  int64_t dx=int64_t(v.x)-t.x,dy=int64_t(v.y)-t.y;
  if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)continue;
  const int64_t value=dx*dx+dy*dy;
  if((candidateSpecial&&!special)||value<distance||(value==distance&&(best<0||t.id<targets[best].id))){best=i;distance=value;special=candidateSpecial;}
 }
 if(best>=0){
  send(int(actions[best]&63)-1,!(v.engineer||v.spy));d.target=targets[best].id;
  if(v.engineer||v.spy){d.follow=1;d.accepted.stage=3;d.accepted.sentFrame=frame;}
 }else if((v.engineer||v.spy)&&frame-r.stableSince<90)r.sentFrame=frame+6;
 else if(v.mission!=0)d.done=1;
 else send(5,true);
 return d;
}
inline Result calculate(const Input&i){Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(!valid(i))return r;r.count=i.count;
 for(int n=0;n<i.count;++n)r.decisions[n]=decide(i.sources[n],i.targets,i.actions[n],i.targetCount);return r;
}
} }
