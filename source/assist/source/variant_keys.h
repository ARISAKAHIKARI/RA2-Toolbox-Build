#pragma once
#include <cstdint>
#include "unload_policy.h"
namespace assist { namespace typekeys {
// Runtime selection is bounded by the live-world roster, not the UI catalogue.
// No allocator, destructor registration or exception-runtime dependency.
struct VariantKeys {
 static constexpr unsigned Capacity=16384,Slots=Capacity*2;
 uint64_t keys[Slots]{};uint32_t epochs[Slots]{},epoch=1;
 unloadpolicy::RosterLookup<Capacity> types;
 void clear(){types.clear();if(++epoch==0){for(auto&e:epochs)e=0;epoch=1;}}
 static unsigned hash(uint64_t key){key^=key>>33;key*=0xff51afd7ed558ccdull;key^=key>>33;return unsigned(key)&(Slots-1);}
 void add(uint32_t type,uint32_t weapon){if(!type)return;types.add(type,0);uint64_t key=(uint64_t(type)<<32)|weapon;unsigned at=hash(key);
  for(unsigned n=0;n<Slots;++n,at=(at+1)&(Slots-1))if(epochs[at]!=epoch||keys[at]==key){keys[at]=key;epochs[at]=epoch;return;}
 }
 bool hasType(uint32_t type)const{return types.find(type)>=0;}
 bool contains(uint32_t type,uint32_t weapon)const{uint64_t key=(uint64_t(type)<<32)|weapon;unsigned at=hash(key);
  for(unsigned n=0;n<Slots;++n,at=(at+1)&(Slots-1)){if(epochs[at]!=epoch)return false;if(keys[at]==key)return true;}return false;
 }
 bool empty()const{return types.count==0;}
};
} }
