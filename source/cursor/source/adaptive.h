#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
// Each standard cursor and minimap variant gets its own robust interval history.
struct AdaptiveTiming {
 struct History {std::array<unsigned,7> samples{};unsigned size=0,head=0;int first=-1,count=0;
  void add(unsigned ms){samples[head]=ms;head=(head+1)%samples.size();size=std::min(unsigned(samples.size()),size+1);}
  unsigned median()const{auto v=samples;std::sort(v.begin(),v.begin()+size);return size?v[size/2]:0;}
 };
 // Host table keys use 0..171; validated bridge ranges use 900+first.
 // Keep timing for both paths, including all 450 possible first frames.
 std::array<History,1350> history{};
 int key=-1,frame=-1;uint32_t changedAt=0;bool boundary=false;
 void resetObservation(){key=frame=-1;boundary=false;changedAt=0;}
 void clear(){history={};resetObservation();}
 void observe(int next,int group,int first,int count,uint32_t now){
  if(group<0||group>=int(history.size())||count<2||next<first||next>=first+count){resetObservation();return;}
  auto& h=history[group];if(h.first!=first||h.count!=count){h={};h.first=first;h.count=count;resetObservation();}
  if(group!=key){key=group;frame=next;changedAt=now;boundary=false;return;}
  uint32_t dt=now-changedAt;
  if(next==frame){if(dt>500)boundary=false;return;}
  bool adjacent=next==first+(frame-first+1)%count;
  // First observation can land halfway through a frame: wait for two boundaries.
  if(adjacent&&boundary&&dt>=3&&dt<=500)h.add(dt);
  boundary=adjacent;frame=next;changedAt=now;
 }
 unsigned samples(int group)const{return group>=0&&group<int(history.size())?history[group].size:0;}
 unsigned duration(int group,unsigned manual,bool enabled)const{
  if(!enabled||samples(group)<3)return manual;
  return std::max(3u,std::min(500u,history[group].median()));
 }
};
