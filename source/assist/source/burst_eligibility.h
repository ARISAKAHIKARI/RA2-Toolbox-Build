#pragma once
#include <cstdint>
namespace assist { namespace bursteligibility {
// YRpp TechnoClass::SpawnOwner (0x2D4), TechnoTypeClass::Spawned (0xD54),
// Spawns (0xD58), SpawnsNumber (0xD5C), MissileSpawn (0xD68).
// Use engine attributes, not an ID blacklist: renamed/modded spawn types and
// temporarily orphaned carrier aircraft remain excluded.
struct Source {bool managed=false,aircraft=false;};
template<class Read> Source inspect(uint32_t object,uint32_t type,int kind,Read read){
 Source s;if(object<0x10000||type<0x10000||kind<0){s.managed=true;return s;}
 uint32_t owner=0,spawns=0;int count=0,speed=0;uint8_t spawned=0,missile=0,inAir=0,height=0,falling=0;
 bool ok=read(object+0x2D4,&owner,4)&&read(type+0xD54,&spawned,1)&&read(type+0xD58,&spawns,4)&&
  read(type+0xD5C,&count,4)&&read(type+0xD68,&missile,1);
 s.managed=!ok||owner||spawned||missile||(spawns&&count>0);
 s.aircraft=kind==0;
 if(kind==2){ok=read(object+0x134,&inAir,1)&&read(object+0x94,&speed,4)&&read(object+0x84,&height,1)&&read(object+0x8D,&falling,1);
  s.aircraft=!ok||inAir||speed==3||height||falling;}
 return s;
}
inline bool automatic(Source s){return !s.managed&&!s.aircraft;}
inline bool held(Source s){return !s.managed;}
}}
