#include "magnetron_drop.h"
static bool rawMagnetronDrop(uint32_t source,uint32_t victim,uint32_t house){
 if(!source||!victim||!sh->magnetronDrop||!rawMagnetron(source,featuretypes::MagnetronDrop)||get<uint32_t>(source+0x2AC)!=victim||
    !worldObjects.contains(victim)||kind(victim)!=2||!live(victim)||get<uint32_t>(victim+0x2B0)!=source||!nativeEnemy(victim,house))return false;
 struct {int x,y,z;} position{get<int>(victim+0x9C),get<int>(victim+0xA0),get<int>(victim+0xA4)};
 uint32_t cell=readMapCell(position.x/256,position.y/256);if(!cell)return false;
 magnetdrop::Ground g;g.lifted=true;g.enemy=true;g.land=get<int>(cell+0xEC);g.bridge=(get<uint32_t>(cell+0x140)&0x100)!=0;
 g.height=position.z-call<int>(0x578080,0x87F7E8,&position);
 // Follow the same cell object chain as native landing, ignoring the victim.
 uint32_t q=get<uint32_t>(cell+0xE4);for(int n=0;q&&n<128;++n){if(q<0x10000)return false;
  if(q!=victim&&worldObjects.contains(q)&&kind(q)==3&&live(q)){g.building=true;break;}
  uint32_t next=get<uint32_t>(q+0x30);if(next==q)return false;q=next;
 }
 return magnetdrop::lethal(g);
}
