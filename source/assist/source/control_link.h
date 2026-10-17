#pragma once
#include <stdint.h>
namespace assist { namespace controllink {
struct View {bool mapped=false,observed=false,stopping=false;int status=0,command=0;uint32_t match=0;};
// A sampler's epoch and a time deadline cannot identify a bridge session.
inline uint32_t identity(View v){return v.mapped&&v.observed?v.match:0;}
inline bool ready(View v){return identity(v)&&!v.stopping&&v.status==1&&v.command==1;}
inline uint32_t mergedIdentity(View v,uint32_t external){const auto id=identity(v);return id?id:(0x80000000u|external);}
inline bool transientPause(bool flag,bool sameHouse,bool housePresent,bool localPlayable){return !flag&&sameHouse&&housePresent&&localPlayable;}
}}
