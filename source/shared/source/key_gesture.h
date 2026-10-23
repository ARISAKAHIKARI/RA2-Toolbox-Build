#pragma once
#include <cstdint>
#include <cstring>
namespace keygesture {
constexpr uint16_t Double=0x8000,Physical=0x7ff;constexpr uint32_t Window=350;
inline uint16_t chord(unsigned vk,const uint8_t*keys){unsigned mods=(keys[16]?1:0)|(keys[17]?2:0)|(keys[18]?4:0);if(vk==16)mods&=~1u;if(vk==17)mods&=~2u;if(vk==18)mods&=~4u;return uint16_t(vk|(mods<<8));}
struct Tracker {
 uint8_t held[256]{},pending[256]{};uint16_t chords[256]{};uint32_t at[256]{},serial[256]{};uint32_t next=0;
 void clear(){uint32_t sequence=next;*this={};next=sequence;}
 void edge(unsigned vk,bool down,bool repeat,uint32_t now,uint16_t code){
  if(vk>=256)return;if(!down){held[vk]=0;return;}if(repeat||held[vk])return;
  bool twice=pending[vk]&&chords[vk]==code&&uint32_t(now-at[vk])<=Window;
  // A different ordinary key interrupts a modifier double tap / key pair.
  if(vk!=16&&vk!=17&&vk!=18&&(vk<160||vk>165))for(unsigned i=0;i<256;++i)if(i!=vk)pending[i]=0;
  held[vk]=twice?3:1;pending[vk]=!twice;chords[vk]=code;at[vk]=now;serial[vk]=++next;
 }
 void sample(const uint8_t*raw,uint32_t now){for(unsigned i=0;i<256;++i)if(!raw[i])edge(i,false,false,now,0);for(unsigned i=0;i<256;++i)if(raw[i])edge(i,true,false,now,chord(i,raw));}
 void decorate(uint8_t*keys)const{for(unsigned i=0;i<256;++i)if(keys[i]&&(held[i]&2))keys[i]|=2;}
 bool qualifies(uint16_t binding)const{unsigned vk=binding&255;return vk&&held[vk]&&(!(binding&Double)||(held[vk]&2));}
};
}
