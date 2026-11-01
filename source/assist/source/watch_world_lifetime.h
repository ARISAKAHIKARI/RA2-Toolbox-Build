#pragma once
#include "world_lifecycle.h"
namespace watchworld {
struct Decision {bool ready=false,clear=false,rebind=false;};
// Display sample epochs can restart after one failed House read. They are
// cache identities, not evidence that the pinned object's world was retired.
struct Lifetime {
 assist::worldlife::Tracker tracker;
 uint32_t pid=0,epoch=0;
 void reset(){tracker.reset();pid=epoch=0;}
 Decision observe(uint32_t process,uint32_t match,const assist::worldlife::Sample&sample){
  Decision out;if(process!=pid){reset();pid=process;out.clear=true;}
  if(!process)return out;
  const auto change=tracker.observe(sample);using C=assist::worldlife::Change;
  if(change==C::Unknown)return out;
  if(change==C::Absent){out.clear=true;epoch=0;return out;}
  if(change==C::Replaced||change==C::Rewind)out.clear=true;
  // Never bind the command bridge's fallback epoch while external data is
  // unavailable. A valid later display observation supplies its own epoch.
  if(!match)return out;
  out.rebind=epoch!=match;epoch=match;out.ready=true;return out;
 }
};
}
