#pragma once
#include <cstdint>
namespace assist { namespace repairdelivery {
struct Observation {bool valid=false,repairing=false,healthy=false;};
// Repair is a toggle. A timer cannot establish that a previous toggle was
// consumed. Keep pending identities until actual state/identity evidence.
template<unsigned Size=4096> struct Pending {
 static_assert(Size&&!(Size&(Size-1)),"power-of-two capacity required");
 struct Slot {uint32_t id=0,pointer=0;uint8_t state=0;}; // 0 empty, 1 used, 2 tombstone
 Slot slots[Size]{};unsigned count=0;
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
  if(!id||!pointer)return false;unsigned i=find(id,true);if(i==Size)return false;
  if(slots[i].state!=1)++count;slots[i]={id,pointer,1};return true;
 }
 template<class Read>void observe(Read read){
  if(!count)return;
  for(auto&r:slots)if(r.state==1){const Observation o=read(r.id,r.pointer);
   if(!o.valid||o.repairing||o.healthy){r.state=2;--count;}
  }
  if(!count)clear();
 }
 void clear(){for(auto&r:slots)r={};count=0;}
};
}}
