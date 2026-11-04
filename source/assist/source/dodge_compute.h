#pragma once
#include <cstdint>
namespace assist { namespace dodgecompute {
constexpr int MaxSources=128,MaxThreats=128;
// No destinations, trajectory scoring or replacement Attack/Guard orders.
// Scatter is selected here; the synchronized native receiver owns movement.
struct Source {
 uint32_t id=0,pointer=0,token=0,job=0,active=0,busy=0,observed=0,freshThreat=0;
 int frame=0,sentFrame=0,quietFrame=0,count=0;
 uint32_t shots[MaxThreats]{};
};
struct Decision {uint32_t scatter=0,release=0;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0;Source sources[MaxSources];};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Decision decisions[MaxSources];};
inline Decision decide(const Source&s){
 Decision d;if(s.count<0||s.count>MaxThreats||s.frame<s.sentFrame)return d;
 if(s.active){
  // New projectiles never replace a pending/turning/moving scatter.
  // Disappearing threats are not evidence that movement has completed.
  if(!s.busy&&((s.observed&&s.frame-s.quietFrame>=2)||(!s.observed&&s.frame-s.sentFrame>=300)))d.release=1;
 }else if(!s.busy&&s.freshThreat&&s.count)d.scatter=1;
 return d;
}
inline Result calculate(const Input&i){Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(i.count<0||i.count>MaxSources)return r;
 r.count=i.count;for(int n=0;n<i.count;++n)r.decisions[n]=decide(i.sources[n]);return r;}
}}
