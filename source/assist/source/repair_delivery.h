#pragma once
#include <cstdint>
namespace assist { namespace repairdelivery {
struct Observation {bool valid=false,repairing=false,healthy=false,uncertain=false;};
// Pending Repair toggles are identity tickets. An unreadable native observation
// never retires one. Dense active slots eliminate a full 16K-slot game-thread
// scan when only a few tickets are waiting for native acknowledgement.
template<unsigned Size=16384> struct Pending {
 static_assert(Size&&!(Size&(Size-1)),"power-of-two capacity required");
 struct Slot {uint32_t id=0,pointer=0,generation=0;uint8_t state=0;}; // 0 empty, 1 used, 2 tombstone
 Slot slots[Size]{};
 unsigned active[Size]{};
 unsigned count=0,cursor=0;
 uint32_t generation=1;
 static constexpr unsigned MaxObservationsPerTick=64;
 unsigned find(uint32_t id,bool insert=false)const{
  unsigned first=Size,s=(id*2654435761u)&(Size-1);
  for(unsigned n=0;n<Size;++n,s=(s+1)&(Size-1)){
   const auto&r=slots[s];
   if(r.generation!=generation||r.state==0)return insert?(first<Size?first:s):Size;
   if(r.state==1&&r.id==id)return s;
   if(r.state==2&&first==Size)first=s;
  }return insert?first:Size;
 }
 bool contains(uint32_t id,uint32_t pointer)const{unsigned i=find(id);return i<Size&&slots[i].pointer==pointer;}
 bool room(uint32_t id)const{return id&&find(id,true)<Size;}
 bool queued(uint32_t id,uint32_t pointer){
  if(!id||!pointer)return false;
  unsigned i=find(id,true);
  if(i==Size)return false;
  auto&r=slots[i];
  if(r.generation!=generation||r.state!=1){active[count++]=i;}
  r.id=id;r.pointer=pointer;r.state=1;r.generation=generation;
  return true;
 }
 // Each callback checks at most budget ACTIVE tickets, never every hash slot.
 // When a ticket is retired, inspect its swapped-in successor before moving
 // the cursor. This preserves fairness and keeps UNKNOWN tickets pending.
 template<class Read>void observeBudgeted(Read read,unsigned budget=MaxObservationsPerTick){
  if(!count)return;
  while(count&&budget--){
   if(cursor>=count)cursor=0;
   const unsigned index=active[cursor];auto&r=slots[index];
   const Observation o=read(r.id,r.pointer);
   if(o.uncertain||(o.valid&&!o.repairing&&!o.healthy)){
    if(++cursor>=count)cursor=0;
    continue;
   }
   const unsigned last=active[count-1];
   active[cursor]=last;
   r.state=2;--count;
   if(cursor>=count)cursor=0;
  }
  if(!count)clear();
 }
 // Retain the old full-pass API for non-game-thread diagnostic consumers.
 template<class Read>void observe(Read read){observeBudgeted(read,Size);}
 // A completed last ticket must not memset 16K slots from the game callback.
 // Generational invalidation resets in O(1), except once upon 32-bit wrap.
 void clear(){count=cursor=0;if(++generation==0){for(auto&r:slots)r={};generation=1;}}
};
}}
