#pragma once
#include <cstdint>
#include <algorithm>
namespace assist { namespace dodgecompute {
constexpr int MaxSources=128,MaxThreats=128;
constexpr uint32_t RetentionMs=5000;
// Callback-local lookup for the raw projectile target / vehicle target and
// destination. A sorted flat table avoids per-unit full threat traversals.
// Merge two keys in original collection order so MaxThreats truncation and
// the position of shot IDs remain identical to the unindexed collector.
class ThreatIndex {
 struct Entry {uint32_t target;int ordinal;};
 static constexpr int Capacity=2*8192;
 Entry entries_[Capacity];int count_=0;
 const Entry*begin(uint32_t target)const{
  return std::lower_bound(entries_,entries_+count_,target,[](const Entry&e,uint32_t t){return e.target<t;});
 }
public:
 void clear(){count_=0;}
 bool add(uint32_t target,int ordinal){
  if(count_==Capacity)return false;
  entries_[count_++]={target,ordinal};return true;
 }
 void seal(){std::sort(entries_,entries_+count_,[](const Entry&a,const Entry&b){return a.target<b.target||(a.target==b.target&&a.ordinal<b.ordinal);});}
 template<class Visit>void visit(uint32_t first,uint32_t second,Visit visit)const{
  const Entry*end=entries_+count_,*a=begin(first),*b=first==second?end:begin(second);
  while((a!=end&&a->target==first)||(b!=end&&b->target==second)){
   const bool haveA=a!=end&&a->target==first,haveB=b!=end&&b->target==second;
   int ordinal;
   if(haveA&&(!haveB||a->ordinal<=b->ordinal)){ordinal=a++->ordinal;}
   else ordinal=b++->ordinal;
   while(a!=end&&a->target==first&&a->ordinal==ordinal)++a;
   while(b!=end&&b->target==second&&b->ordinal==ordinal)++b;
   if(!visit(ordinal))break;
  }
 }
};

inline bool retentionExpired(uint32_t now,uint32_t lastSeenAt){return int32_t(now-lastSeenAt)>=0&&uint32_t(now-lastSeenAt)>RetentionMs;}
// No destinations, trajectory scoring or replacement Attack/Guard orders.
// Scatter is selected here; the synchronized native receiver owns movement.
struct Source {
 uint32_t id=0,pointer=0,token=0,job=0,active=0,busy=0,observed=0,freshThreat=0;
 int frame=0;uint32_t sentAt=0;int quietFrame=0,count=0;
 uint32_t shots[MaxThreats]{};
};
struct Decision {uint32_t scatter=0,release=0;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0;Source sources[MaxSources];};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Decision decisions[MaxSources];};
inline Decision decide(const Source&s,uint32_t now){
 Decision d;if(s.count<0||s.count>MaxThreats||s.frame<0)return d;
 if(s.active){
  // New projectiles never replace a pending/turning/moving scatter.
  // Disappearing threats are not evidence that movement has completed.
  if(!s.busy&&((s.observed&&s.frame-s.quietFrame>=2)||(!s.observed&&retentionExpired(now,s.sentAt))))d.release=1;
 }else if(!s.busy&&s.freshThreat&&s.count)d.scatter=1;
 return d;
}
inline Result calculate(const Input&i){Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(i.count<0||i.count>MaxSources)return r;
 r.count=i.count;for(int n=0;n<i.count;++n)r.decisions[n]=decide(i.sources[n],i.tick);return r;}
}}
