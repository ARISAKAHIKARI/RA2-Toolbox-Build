#pragma once
#include <cstdint>
#include <cmath>
#include "enemy_markers.h"
// Host-only read/display data. No engine calls, hooks, orders or Shared fields.
namespace external { namespace status {
constexpr int Maximum=2048;
enum Options : uint32_t { Parasite=1, CrateBuffs=2 };
enum Buffs : uint32_t { Firepower=1, Armor=2, Speed=4 };
struct Marker {
 uint32_t object=0,id=0,type=0,owner=0,parasite=0,parasiteOwner=0,parasiteColor=0,buffs=0;
 int kind=-1,x=0,y=0;
};
struct Frame {uint32_t match=0,tick=0,house=0;int frame=-1,count=0,width=0,height=0;Marker items[Maximum]{};};
inline bool fresh(const Frame&f,uint32_t match,uint32_t now){return f.match==match&&f.tick&&uint32_t(now-f.tick)<250&&f.count>=0&&f.count<=Maximum;}
inline int footKind(uint32_t v){return v==0x7F5C70?2:v==0x7EB058?1:v==0x7E22A4?0:-1;}
template<class Read> bool inspect(Read read,uint32_t p,int kind,uint32_t options,Marker&out){
 out.parasite=out.parasiteOwner=out.parasiteColor=out.buffs=0;
 if(kind<0||kind>2)return false; // Foot fields must never be read from buildings.
 auto get=[&](uint32_t at,auto fallback){auto value=fallback;return read(at,&value,sizeof value)?value:fallback;};
 if(get(p+0x11C,uint32_t(0xFFFFFFFF)))return false;
 if(options&CrateBuffs){
  // Original crate reward stores: Armor 482E79 -> +158, Speed 48306C ->
  // +580, Firepower 483248 -> +160. These are separate from Veterancy +150.
  auto boosted=[&](uint32_t offset){double value=get(p+offset,1.0);return std::isfinite(value)&&value>1.000001&&value<=1024.0;};
  if(boosted(0x160))out.buffs|=Firepower;if(boosted(0x158))out.buffs|=Armor;if(boosted(0x580))out.buffs|=Speed;
 }
 if(options&Parasite){
  uint32_t q=get(p+0x694,uint32_t(0));
  if(q>=0x10000&&q!=p&&footKind(get(q,uint32_t(0)))>=0&&get(q+0x10,uint32_t(0))&&get(q+0x6C,0)>0&&get(q+0x90,uint8_t(0))){
   // The attacker can be Limbo/off-map while inside the victim. Validate
   // ParasiteImUsing and both ends of its owner/victim relation instead.
   uint32_t link=get(q+0x69C,uint32_t(0));
   if(link>=0x10000&&get(link+0x24,uint32_t(0))==q&&get(link+0x28,uint32_t(0))==p){
    uint32_t owner=get(q+0x21C,uint32_t(0));int index=get(owner+0x30,-1);uint8_t rgb[3]{};
    if(owner>=0x10000&&index>=0&&index<32&&read(owner+22265,rgb,3)){
     out.parasite=q;out.parasiteOwner=owner;out.parasiteColor=0xFF000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];
    }
   }
  }
 }
 return out.parasite||out.buffs;
}
}}
