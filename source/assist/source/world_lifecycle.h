#pragma once
#include <stdint.h>
#include <stddef.h>
namespace assist { namespace worldlife {
struct Sample {
 uint32_t scenario=0,cells=0;int count=0,frame=-1,session=-1;uint8_t running=0;bool readable=false;
 bool valid()const{return readable&&scenario>=0x10000&&cells>=0x10000&&count>0&&count<=512*512&&frame>=0&&frame<0x7fffffff&&(session==0||session==3||session==4||session==5);}
 bool same(const Sample&o)const{return scenario==o.scenario&&cells==o.cells&&count==o.count&&session==o.session;}
};
template<class Read>Sample capture(Read read){Sample s;s.readable=
 read(0xA8B230,&s.scenario,4)&&read(0xA8B238,&s.session,4)&&
 read(0x87F924,&s.cells,4)&&read(0x87F928,&s.count,4)&&
 read(0xA8ED84,&s.frame,4)&&read(0xA8E9A0,&s.running,1);return s;}
enum class Change:uint8_t { Unknown, Absent, Start, Same, Replaced, Rewind };
struct Tracker {
 Sample world{};bool bound=false;
 Change observe(const Sample&s){
  if(!s.readable)return Change::Unknown;
  // Invalid intermediate loading values are not proof that the previous
  // world's owned sites disappeared. A fully empty scenario/map is.
  if(!s.valid()){
   if(!s.running&&!s.scenario&&!s.cells&&!s.count){bound=false;world={};return Change::Absent;}
   return Change::Unknown;
  }
  const auto result=!bound?Change::Start:!world.same(s)?Change::Replaced:s.frame<world.frame?Change::Rewind:Change::Same;
  world=s;bound=true;return result;
 }
 void reset(){world={};bound=false;}
};
// House eligibility controls commands, not ownership of game code or vtables.
enum class Authority:uint8_t { Unknown, ViewOnly, Command };
template<class Read>Authority authority(Read read,uint32_t&house){
 uint32_t observer=0,items=0;int count=-1,capacity=-1;uint8_t ended[4]{};
 if(!read(0xA83D4C,&house,4)||!read(0xAC1198,&observer,4))return Authority::Unknown;
 if(house<0x10000)return Authority::ViewOnly;
 if(!read(house+0x1F5,ended,sizeof ended)||!read(0xA8022C,&items,4)||!read(0xA80230,&capacity,4)||!read(0xA80238,&count,4)||
    count<0||count>32||capacity<count||(count&&items<0x10000))return Authority::Unknown;
 uint32_t houses[32]{};if(count&&!read(items,houses,size_t(count)*4))return Authority::Unknown;
 bool member=false;for(int i=0;i<count;++i)if(houses[i]==house)member=true;
 return member&&house!=observer&&!ended[0]&&!ended[1]&&!ended[3]?Authority::Command:Authority::ViewOnly;
}
}}
