extern "C" int GuardedObjectAir(unsigned,int*);
#include "magnetron_drop.h"
static bool rawMagnetronDrop(uint32_t source,uint32_t victim,uint32_t house){
 if(!source||!victim||!sh->magnetronDrop||!rawMagnetron(source,featuretypes::MagnetronDrop)||get<uint32_t>(source+0x2AC)!=victim||
    !worldObjects.contains(victim)||kind(victim)!=2||!live(victim)||get<uint32_t>(victim+0x2B0)!=source||!nativeEnemy(victim,house))return false;
 // This independent toggle explicitly applies to every owned active beam,
 // including manual attacks. Manual pause belongs only to auto acquisition.
 struct {int x,y,z;} position{get<int>(victim+0x9C),get<int>(victim+0xA0),get<int>(victim+0xA4)};
 uint32_t cell=readMapCell(position.x/256,position.y/256);if(!cell)return false;
 magnetdrop::Ground g;g.lifted=true;g.enemy=true;g.land=get<int>(cell+0xEC);g.bridge=(get<uint32_t>(cell+0x140)&0x100)!=0;
 g.height=position.z-call<int>(0x578080,0x87F7E8,&position);
 uint32_t type=typeOf(victim,2);if(!type)return false;
 g.speed=get<int>(type+0x67C);g.zone=get<int>(type+0x5B4);
 if(g.land>=0&&g.land<12&&g.speed>=0&&g.speed<8){float speed=0;
  if(read(0x89EA40u+uint32_t(g.land*9+g.speed)*4,&speed,4)&&std::isfinite(speed)&&speed>=0&&speed<1000000){g.mobilityKnown=true;g.passable=speed>0;}
 }
 // Cell content (not screen projection) also contains TerrainClass, which
 // is absent from TechnoClass::Array. Trees and ore pillars share this class.
 uint32_t q=get<uint32_t>(cell+0xE4),seen[128]{};int count=0;
 while(q){
  if(count==128||q<0x10000)return false;for(int n=0;n<count;++n)if(seen[n]==q)return false;seen[count++]=q;
  uint32_t table=0,next=0;if(!read(q,&table,4)||!read(q+0x30,&next,4))return false;
  if(q!=victim&&q!=source){
   if(table==0x7F522Cu&&live(q)&&!get<bool>(q+0xCD))g.terrain=true;
   else if(worldObjects.contains(q)&&live(q)){
    if(kind(q)==3)g.building=true;
    else if(kind(q)==2&&!get<uint32_t>(q+0x11C)&&!get<uint32_t>(q+0x2B0)&&!get<bool>(q+0x427)&&
            !get<bool>(q+0x134)&&!get<bool>(q+0x84)&&!get<bool>(q+0x8D)&&get<int>(q+0x94)!=3){
     struct {int x,y,z;} below{get<int>(q+0x9C),get<int>(q+0xA0),get<int>(q+0xA4)};
     // Even low hovering objects (below IsInAir's threshold) are excluded.
     int air=0; // native height/air queries are read-only
     if(below.x/256==position.x/256&&below.y/256==position.y/256&&
        below.z==call<int>(0x578080,0x87F7E8,&below)&&GuardedObjectAir(q,&air)&&!air)g.groundVehicle=true;
    }
   }
  }
  q=next;
 }
 return magnetdrop::opportunity(g);
}
