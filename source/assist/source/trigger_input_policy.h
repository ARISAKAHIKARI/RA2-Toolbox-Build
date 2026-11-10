#pragma once
#include <cstdint>
namespace assist::triggerinput {
constexpr uint32_t Entry=0x6DD8B0,Tag=64;
constexpr uint8_t Prefix[]={0x81,0xEC,0x40,0x02,0,0,0xB0,1,0x53,0x55,0x56,0x8B,0xF1};
inline bool blocked(int action,bool input,bool camera){
 // Unlock/ZoomOut retire locks begun before activation; keep them. Map bounds
 // (ResizePlayerView) affect the map and are deliberately outside this filter.
 return (input&&action==46)||(camera&&(action==48||action==49||action==112||action==145));
}
}
