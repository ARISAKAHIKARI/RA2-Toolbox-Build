#pragma once
#include <cstdint>
namespace assist { namespace repairdelivery {
struct Observation {bool valid=false,repairing=false,healthy=false;};
struct ObservationWork {unsigned scanned=0,sampled=0,retired=0;};
constexpr unsigned ObservationReadBudget=32,ObservationSlotBudget=256;
// Repair is a toggle. A timer cannot establish that a previous toggle was
// consumed. Keep pending identities until actual state/identity evidence.
template<unsigned Size=4096> struct Pending {
 static_assert(Size&&!(Size&(Size-1)),"power-of-two capacity required");
 struct Slot {uint32_t id=0,pointer=0;uint8_t state=0;}; // 0 empty, 1 used, 2 tombstone
 Slot slots[Size]{};unsigned count=0,cursor=0;
 unsigned find(uint32_t id,bool insert=false)const{
  unsigned first=Size,s=(id*2654435761u)&(Size-1);
  for(unsigned n=0;n<Size;++n,s=(s+1)&(Size-1)){
   const auto&r=slots[s];
   if(r.state==1&&r.id==id)return s;
   if(r.state==2&&first==Size)first=s;
   if(r.state==0)return insert?(first<Size?first:s):Size;
  }return insert?first:Size;
 }
 bool contains(uint32_t id,uint32_t pointer)const{unsigned i=find(id);return i<Size&&slots[i].pointer==pointer;}
 bool room(uint32_t id)const{return id&&find(id,true)<Size;}
 bool queued(uint32_t id,uint32_t pointer){
  if(!id||!pointer)return false;
  unsigned i=find(id,true);if(i==Size)return false;
  if(slots[i].state!=1)++count;
  slots[i]={id,pointer,1};return true;
 }
 // Resume a bounded, read-only sweep on the next existing game-thread tick.
 // Budgets count work, not elapsed milliseconds. Unvisited entries remain
 // pending: a delayed observation must NEVER authorize another repair toggle.
 // The callback must not mutate this table. It revalidates id/pointer/owner
 // against the current world, rather than reusing a prior tick's observation.
 template<class Read>ObservationWork observe(Read read,
   unsigned maxReads=ObservationReadBudget,unsigned maxSlots=ObservationSlotBudget){
  ObservationWork work{};
  if(!count||!maxReads||!maxSlots)return work;
  const unsigned limit=maxSlots<Size?maxSlots:Size;
  while(count&&work.scanned<limit&&work.sampled<maxReads){
   auto&r=slots[cursor];cursor=(cursor+1)&(Size-1);++work.scanned;
   if(r.state!=1)continue;
   ++work.sampled;const Observation o=read(r.id,r.pointer);
   if(!o.valid||o.repairing||o.healthy){r.state=2;--count;++work.retired;}
  }
  if(!count)clear(); // Local bookkeeping only; no additional game observations.
  return work;
 }
 void clear(){for(auto&r:slots)r={};count=cursor=0;}
};
}}
