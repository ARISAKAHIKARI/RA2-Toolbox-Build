#pragma once
#include <cstdint>
#include <climits>
#include <algorithm>
namespace assist { namespace missilecompute {
constexpr int MaxSources=64,MaxTargets=64,MaxOrders=64,LookAhead=30;
enum Mode { Smart=0, Even=1 };
enum Action { Attack=1, Hold=0, Release=5 };
struct Source {uint32_t id=0,pointer=0,token=0,current=0,pending=0;int cooldown=0,held=0,pendingMission=-1;};
struct Target {uint32_t id=0,pointer=0;int hp=0;uint32_t threatening=0;};
// eta is prediction only; close is the original IsCloseEnough result.
struct Edge {int damage=0;uint32_t distance=0;int eta=0,close=1;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0,targetCount=0,mode=Smart;
 Source sources[MaxSources];Target targets[MaxTargets];Edge edges[MaxSources][MaxTargets];};
struct Assignment {int source=0,target=-1,action=Attack;};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Assignment orders[MaxOrders];};
inline bool valid(const Input&i){return i.frame>=0&&i.count>=0&&i.count<=MaxSources&&i.targetCount>=0&&i.targetCount<=MaxTargets&&(i.mode==Smart||i.mode==Even);}
inline Result calculate(const Input&i){
 Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(!valid(i))return r;
 int assigned[MaxSources],number[MaxTargets]{},options[MaxSources]{};int64_t damage[MaxTargets]{};
 bool nearby[MaxSources]{},busy[MaxSources]{};
 std::fill(assigned,assigned+MaxSources,-1);
 for(int s=0;s<i.count;++s){
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].hp>0&&i.edges[s][t].damage>0){
   nearby[s]=true;if(std::max(i.edges[s][t].eta,i.sources[s].cooldown)<=LookAhead)++options[s];
  }
  // Pending events reserve their shooters. Existing auto-acquired targets do
  // not: excess shooters need an explicit native Sleep to preserve ammunition.
  busy[s]=i.sources[s].pendingMission>=0;
  if(busy[s]&&i.sources[s].pendingMission==Attack)for(int t=0;t<i.targetCount;++t)
   if(i.sources[s].pending==i.targets[t].id&&i.edges[s][t].damage>0){assigned[s]=t;++number[t];damage[t]+=i.edges[s][t].damage;break;}
 }
 for(int pass=0;pass<i.count;++pass){int bs=-1,bt=-1,br=INT_MAX;
  for(int s=0;s<i.count;++s)if(!busy[s]&&assigned[s]<0)for(int t=0;t<i.targetCount;++t){
   const auto&e=i.edges[s][t];const auto&target=i.targets[t];int ready=std::max(e.eta,i.sources[s].cooldown);
   if(e.damage<=0||target.hp<=0||ready>LookAhead||(i.mode==Smart&&damage[t]>=target.hp))continue;
   bool better=bt<0;
   if(!better){
    const int64_t left=i.mode==Even?number[t]:damage[t]*i.targets[bt].hp;
    const int64_t right=i.mode==Even?number[bt]:damage[bt]*target.hp;
    if(left!=right)better=left<right;
    else if(target.threatening!=i.targets[bt].threatening)better=target.threatening>i.targets[bt].threatening;
    else if(ready!=br)better=ready<br;
    else if(options[s]!=options[bs])better=options[s]<options[bs];
    else if((i.sources[s].current==target.id)!=(i.sources[bs].current==i.targets[bt].id))better=i.sources[s].current==target.id;
    else if(e.distance!=i.edges[bs][bt].distance)better=e.distance<i.edges[bs][bt].distance;
    else if(target.id!=i.targets[bt].id)better=target.id<i.targets[bt].id;
    else better=i.sources[s].id<i.sources[bs].id;
   }
   if(better){bs=s;bt=t;br=ready;}
  }
  if(bs<0)break;assigned[bs]=bt;++number[bt];damage[bt]+=i.edges[bs][bt].damage;
 }
 // Prioritize holding reserves; admission still shares the global frame budget.
 for(int phase=0;phase<2;++phase)for(int s=0;s<i.count;++s){if(busy[s])continue;
  int t=assigned[s],action=Release;
  if(t>=0)action=i.edges[s][t].close?Attack:Hold;
  else if(nearby[s])action=Hold;
  if((phase==0)!=(action==Hold))continue;
  if(action==Hold&&i.sources[s].held)continue;
  if(action==Release&&!i.sources[s].held)continue;
  if(action==Attack&&!i.sources[s].held&&i.sources[s].current==i.targets[t].id)continue;
  r.orders[r.count++]={s,t,action};
 }
 return r;
}
} }
