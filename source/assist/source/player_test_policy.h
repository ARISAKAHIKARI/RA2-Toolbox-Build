#pragma once
#include <stdint.h>
namespace assist { namespace playertest {
inline bool sameName(const char*a,const char*b){for(unsigned i=0;i<24;++i){unsigned x=(unsigned char)a[i],y=(unsigned char)b[i];if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;if(!x)return true;}return false;}
inline bool eligible(int index,bool passive,bool spectator,bool defeated,const char*type){
 return index>=0&&index<32&&!passive&&!spectator&&!defeated&&type&&
  !sameName(type,"Neutral")&&!sameName(type,"Special")&&!sameName(type,"Civilian")&&!sameName(type,"CivilianSide")&&!sameName(type,"Police");
}
inline bool manualMission(int mission){return mission==1||mission==2;} // explicit move/attack only
}}
