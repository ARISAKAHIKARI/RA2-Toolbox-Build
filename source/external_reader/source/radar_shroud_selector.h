#pragma once
#include <array>
#include <cstdint>

namespace external { namespace shroudselector {
// Native Cell::DrawShroud (0x4801F0) calls 0x6D8700 for each cell. The two
// source bytes are live exploration bits, not the previously drawn frame bytes.
struct Bits {uint8_t shroud=0,fog=0;};
template<class Read>bool readLookup(Read read,std::array<int8_t,256>&lookup){
 if(!read(0x7F4194,lookup.data(),lookup.size()))return false;
 for(int8_t frame:lookup)if(frame<-2||frame>46)return false;
 return true;
}
template<class Fetch>int8_t frame(int x,int y,bool fog,const std::array<int8_t,256>&lookup,Fetch fetch){
 Bits current{};if(!fetch(x,y,current))return -2;
 uint8_t value=fog?current.fog:current.shroud;
 unsigned presence=fog?0x03u:0x18u,active=fog?0x02u:0x08u;
 if(!(value&presence))return -2;if(!(value&active))return -1;
 // The bit positions and neighbor order match the original selector's
 // signed 256-entry table; masks apply at a flat cell's screen position.
 static constexpr struct {int dx,dy;unsigned bit;} neighbors[]={
  {-1,-1,64},{0,-1,128},{1,-1,1},{-1,0,32},{1,0,2},
  {-1,1,16},{0,1,8},{1,1,4}};
 unsigned index=0;for(const auto&n:neighbors){Bits other{};
  if(!fetch(x+n.dx,y+n.dy,other)||!((fog?other.fog:other.shroud)&active))index|=n.bit;
 }
 return lookup[index];
}
}}
