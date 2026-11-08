#pragma once
#include "world_frame.h"
#include <unordered_set>

namespace worldframe {
// These layouts are inherited from the supported YR readers, not new native
// entry points. Capture discovers pointer dependencies without invoking any
// engine routine or modifying the game. Per-feature projections run after seal.
template<class Read>class CaptureGraph {
 Snapshot&frame_;Read live_;
 std::unordered_set<uint32_t>actors_,types_,small_,houses_;
 std::vector<uint32_t>queue_;
 uint32_t demand_=Base;bool failed_=false;
 bool working()const{return !failed_&&!frame_.overflow();}
 template<class T>T value(uint32_t at,T fallback={}){T v{};return frame_.capture(live_,at,&v,sizeof v)?v:fallback;}
 void block(uint32_t p,size_t n){if(working()&&range(p,n))frame_.prefetch(live_,p,n);}
 void enqueue(uint32_t p){if(!working()||p<0x10000||p>=0x80000000u||actors_.count(p))return;if(queue_.size()>=65536){failed_=true;++frame_.counters.overflows;return;}actors_.insert(p);queue_.push_back(p);}
 void auxiliary(uint32_t p,size_t n){if(working()&&p>=0x10000&&small_.insert(p).second)block(p,n);}
 void animation(uint32_t p){
  if(!working()||p<0x10000||!small_.insert(p).second)return;
  block(p,0x400);auxiliary(value<uint32_t>(p+0xC8),0x400);enqueue(value<uint32_t>(p+0xCC));
 }
 void house(uint32_t p){
  if(!working()||p<0x10000||!houses_.insert(p).second)return;block(p,0x5800);
  auxiliary(value<uint32_t>(p+0x34),0x400);
  for(uint32_t offset:{0x53ACu,0x53B0u,0x53B4u,0x53B8u,0x53BCu,0x53CCu})auxiliary(value<uint32_t>(p+offset),0x80);
 }
 void type(uint32_t p){
  if(!working()||p<0x10000||!types_.insert(p).second)return;block(p,0x2000);auxiliary(value<uint32_t>(p+0x60),128);
  if(demand_&Ore){std::vector<uint32_t>docks;if(vector(p+0x3E8,256,docks))for(uint32_t dock:docks)block(dock,0x2000);}
  if(!(demand_&Combat))return;
  for(unsigned slot=0;slot<18;++slot)for(uint32_t base:{0x898u,0xA94u}){
   if(!working())return;
   const uint32_t weapon=value<uint32_t>(p+base+slot*28);if(weapon<0x10000||!small_.insert(weapon).second)continue;
   block(weapon,0x200);auxiliary(value<uint32_t>(weapon+0xA0),0x400);auxiliary(value<uint32_t>(weapon+0xAC),0x200);
  }
 }
 void actor(uint32_t p){
  block(p,0x800);const uint32_t vt=value<uint32_t>(p);
  const int kind=vt==0x7F5C70?2:vt==0x7EB058?1:vt==0x7E3EBC?3:vt==0x7E22A4?0:-1;
  if(kind<0)return;type(value<uint32_t>(p+(kind==3?0x520:kind==1?0x6C0:0x6C4)));house(value<uint32_t>(p+0x21C));
  if(kind==3&&(demand_&Visual)){
   for(unsigned i=0;i<21;++i)animation(value<uint32_t>(p+0x55C+i*4));
   for(unsigned i=0;i<8;++i)animation(value<uint32_t>(p+0x5C8+i*4));
  }
  // Original passenger links and object references, including targets not in
  // the main Techno inventory. Cycles/address duplicates enter the queue once.
  if(demand_&(Passengers|Visual))for(uint32_t at:{0x30u,0x118u,0x11Cu})enqueue(value<uint32_t>(p+at));
  if(demand_&(Combat|Ore))for(uint32_t at:{0x218u,0x2ACu,0x2B4u,0x5A4u})enqueue(value<uint32_t>(p+at));
  const uint32_t radios=value<uint32_t>(p+0xE4);const int radioCount=value<int>(p+0xE8,-1);
  if((demand_&(Passengers|Ore|Combat))&&radios>=0x10000&&radioCount>0&&radioCount<=256){block(radios,size_t(radioCount)*4);for(int i=0;i<radioCount;++i)enqueue(value<uint32_t>(radios+i*4));}
  if(demand_&(Combat|Visual))for(uint32_t offset:{0x274u,0x278u}){
   uint32_t temporal=value<uint32_t>(p+offset);std::unordered_set<uint32_t>seen;
   for(unsigned i=0;temporal>=0x10000&&i<52&&seen.insert(temporal).second;++i){block(temporal,0x60);temporal=value<uint32_t>(temporal+0x44);}
  }
 }
public:
 CaptureGraph(Snapshot&frame,Read live):frame_(frame),live_(live){}
 bool vector(uint32_t address,int limit,std::vector<uint32_t>&out){
  struct Vector {uint32_t vt,items;int capacity;uint32_t flags;int count,step;}v{};
  out.clear();++frame_.counters.rosterCaptures;
  if(!frame_.capture(live_,address,&v,sizeof v)||v.count<0||v.count>limit||v.capacity<v.count||v.capacity>262144||(v.count&&v.items<0x10000))return false;
  out.resize(size_t(v.count));return !v.count||frame_.capture(live_,v.items,out.data(),out.size()*4);
 }
 bool capture(uint32_t demand,const std::vector<uint32_t>&requests){
  demand_=demand|Base;const bool visuals=(demand_&Visual)!=0;
  // Global fields are one read per overlapping page, regardless of which
  // collector originally owned them. No Selected object list is frozen here.
  for(uint32_t p:{0xA80208u,0xA83CE0u,0xA8B230u,0xA8BC60u,0xA8E348u,0xA8ED84u,0x8871E0u,0x887324u,0x8A00A4u,0x8B4108u,0xAC1198u,0xB0CD48u})block(p,128);
  block(0x87F7E8,0x1500);block(frame_.key.scenario,0x1300);house(frame_.key.house);
  auxiliary(value<uint32_t>(0x887324),0xD0);auxiliary(value<uint32_t>(0x8871E0),0x1000);
  std::vector<uint32_t>list;
  for(uint32_t table:{0xA8EC78u,0xA8EB40u,0x8B4108u}){
   if(table==0x8B4108&&!(demand_&Ore))continue;
   if(!vector(table,16384,list))return false;for(uint32_t p:list)enqueue(p);
  }
  if(!vector(0xA80228,32,list))return false;for(uint32_t p:list)house(p);
  if(demand_&(Bullets|Visual)){
   if(!vector(0xA8ED40,16384,list))return false;
   for(uint32_t p:list){if(!working())return false;block(p,0x200);auxiliary(value<uint32_t>(p+0xAC),0x400);enqueue(value<uint32_t>(p+0xB0));enqueue(value<uint32_t>(p+0x10C));}
  }
  // The map pointer inventory is shared; mutable Cell pages are selected by
  // actual consumer demand, not an unconditional per-frame whole-map sweep.
  if((demand_&(Visual|Ore))&&frame_.key.cellCount&&frame_.key.cellCount<=512u*512u)block(frame_.key.cells,size_t(frame_.key.cellCount)*4);
  if(demand_&Ore){
   for(uint32_t table:{0xA83CE0u,0xA83D80u,0xB0F4E8u}){
    if(!vector(table,4096,list))continue;
    for(uint32_t p:list){if(!working())return false;if(table==0xA83CE0)type(p);else block(p,0x400);
     if(table==0xB0F4E8)auxiliary(value<uint32_t>(p+0xE0),0x400);}
   }block(0x89EA40,12*9*sizeof(float));
  }
  if(visuals){
   for(uint32_t table:{0xA8E988u,0xA8E9A8u,0xA83DC8u,0xA80208u,0x8B4190u,0xABC878u,0xB04A60u,0xA8EC38u,0xAC1678u,0x88A0F0u,0xABCB78u,0x887388u,0x8B3D10u,0xB04DA8u,0x8A0E88u}){
    if(!vector(table,65536,list))continue;
    for(uint32_t p:list){if(!working())return false;block(p,0x400);
     if(table==0xA8E988)auxiliary(value<uint32_t>(p+0xC8),0x400);
     if(table==0xA8E9A8)animation(p);
     if(table==0x887388){auxiliary(value<uint32_t>(p+0x104),0x400);house(value<uint32_t>(p+0x10C));}
     if(table==0x8B3D10){const int count=value<int>(p+0x6C),capacity=value<int>(p+0x64);const uint32_t items=value<uint32_t>(p+0x60);
      if(count>0&&count<=64&&capacity>=count&&capacity<=4096){block(items,size_t(count)*16);for(int i=0;i<count;++i)auxiliary(value<uint32_t>(items+i*16),0x400);}}
     if(table==0xA83DC8||table==0xA80208)auxiliary(value<uint32_t>(p+0xAC),0x400);
     if(table==0xA8EC38){const int count=value<int>(p+0x148);if(count>=3&&count<=6)block(value<uint32_t>(p+0x14C),size_t(count)*8);}
    }
   }
   for(uint32_t layer=0;layer<5;++layer)if(vector(0x8A0360+layer*24,65536,list))for(uint32_t p:list)enqueue(p);
   block(0x89C3B0,96);for(unsigned i=0;i<24;++i)auxiliary(value<uint32_t>(0x89C3B0+i*4),0x114);
  }
  for(size_t i=0;i<queue_.size();++i){if(!working())return false;actor(queue_[i]);}
  for(uint32_t page:requests){if(!working())return false;block(page,PageSize);}
  return working();
 }
};
}
