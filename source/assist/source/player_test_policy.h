#pragma once
#include <stdint.h>
namespace assist { namespace playertest {
// Product availability, shared by host and Runtime; not a user setting.
constexpr bool Available=true;
// Direct-packet sale is suspended, including old settings and stale requests.
constexpr bool SellAvailable=false;
// One explicit button request, never a persistent automatic capability.
constexpr bool DestructAvailable=true;
// Player-context replacement is independent of manual event submission.
// Keep the crashing path disabled, including stale shared-memory requests.
constexpr bool SwitchAvailable=false;
inline bool sameName(const char*a,const char*b){for(unsigned i=0;i<24;++i){unsigned x=(unsigned char)a[i],y=(unsigned char)b[i];if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;if(!x)return true;}return false;}
inline bool eligible(int index,bool passive,bool spectator,bool defeated,const char*type){
 return index>=0&&index<32&&!passive&&!spectator&&!defeated&&type&&
  !sameName(type,"Neutral")&&!sameName(type,"Special")&&!sameName(type,"Civilian")&&!sameName(type,"CivilianSide")&&!sameName(type,"Police");
}
inline bool manualMission(int mission){return mission==1||mission==2;} // explicit move/attack only
}}

