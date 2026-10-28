#pragma once
#include <array>
#include <cstdint>
namespace external { namespace livepalette {
struct Sample {std::array<uint32_t,256> colors{};bool live=false;};
// Content comparison, not pointer identity. A changing or unreadable sample
// never replaces the last coherent palette; its caller must hide stale art.
template<class Resolve>bool refresh(std::array<uint32_t,256>&colors,bool&wasLive,uint64_t&revision,bool&changed,Resolve resolve){
 changed=false;Sample a,b;
 if(!resolve(a)||!resolve(b)||a.colors!=b.colors||a.live!=b.live||(wasLive&&!a.live))return false;
 changed=colors!=a.colors;if(changed){colors=a.colors;++revision;}wasLive=a.live;return true;
}
}}
