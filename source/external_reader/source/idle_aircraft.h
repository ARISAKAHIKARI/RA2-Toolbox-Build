#pragma once
#include "../../assist/source/idle_aircraft_policy.h"
#include <stdint.h>
namespace external { namespace aircraft {
// Raw YRpp fields only. No virtual query, pathfinder, mission change or RNG.
template<class Read> struct Inspect {
 Read read;
 template<class T>T get(uint32_t a,T fallback={}){T value{};return read(a,&value,sizeof value)?value:fallback;}
 bool valid(uint32_t p,uint32_t house){return p>=0x10000&&get<uint32_t>(p+0x10)!=0&&get<uint32_t>(p)==0x7E22A4&&get<uint32_t>(p+0x21C)==house&&get<int>(p+0x6C)>0&&get<uint8_t>(p+0x74)&&!get<uint8_t>(p+0x81)&&!get<uint32_t>(p+0x11C)&&!get<uint8_t>(p+0x6CA);}
 bool airport(uint32_t p,uint32_t type,uint32_t house){
  if(p<0x10000||get<uint32_t>(p)!=0x7E3EBC||get<uint32_t>(p+0x21C)!=house||get<int>(p+0x6C)<=0)return false;
  uint32_t building=get<uint32_t>(p+0x520),items=get<uint32_t>(type+0x3EC);int count=get<int>(type+0x3F8,-1),capacity=get<int>(type+0x3F0,-1);
  if(items<0x10000||count<0||count>256||capacity<count||capacity>65536)return false;
  for(int n=0;n<count;++n)if(get<uint32_t>(items+n*4)==building)return true;return false;
 }
 bool idle(uint32_t p,uint32_t house){
  if(!valid(p,house)||get<uint8_t>(p+0x1C8))return false;uint32_t type=get<uint32_t>(p+0x6C4);if(type<0x10000)return false;
  assist::idleaircraft::View v;v.valid=true;v.ammo=get<int>(p+0x2FC,-1);v.maximum=get<int>(type+0x684,-1);v.mission=get<int>(p+0xAC,-1);v.queued=get<int>(p+0xB4,-1);v.airborne=get<uint8_t>(p+0x134)!=0;v.target=get<uint32_t>(p+0x2B4)!=0;
  uint32_t dest=get<uint32_t>(p+0x5A4);v.returning=airport(dest,type,house);
  uint32_t links=get<uint32_t>(p+0xE4);int count=get<int>(p+0xE8,-1);if(links>=0x10000&&count>=0&&count<=64)for(int i=0;i<count;++i)if(airport(get<uint32_t>(links+i*4),type,house)){v.docked=true;break;}
  if(!v.airborne&&v.returning)v.docked=true;
  return assist::idleaircraft::idle(v);
 }
};
}}
