#include "../../external_reader/source/radar_shroud_assets.h"
#include <cassert>
#include <iostream>
#include <map>
using external::voxel::Image;
namespace masks=external::shroudassets;
static void set16(std::vector<uint8_t>&b,size_t at,uint16_t v){b.at(at)=uint8_t(v);b.at(at+1)=uint8_t(v>>8);}
static void set32(std::vector<uint8_t>&b,size_t at,uint32_t v){set16(b,at,uint16_t(v));set16(b,at+2,uint16_t(v>>16));}
static std::vector<uint8_t> shape(){
 constexpr size_t header=8+2*24,area=60*30;
 std::vector<uint8_t>b(header+2*area,0);std::fill(b.begin()+header,b.end(),0xFE);set16(b,2,60);set16(b,4,30);set16(b,6,2);
 for(int frame=0;frame<2;++frame){size_t at=8+size_t(frame)*24;set16(b,at+4,60);set16(b,at+6,30);set32(b,at+8,0xCCCC0000);set32(b,at+20,uint32_t(header+size_t(frame)*area));
  for(int y=0;y<30;++y)for(int x=0;x<60;++x){int half=std::min(y,29-y);
   if(std::abs(x-29)<=half*2)b[header+size_t(frame)*area+size_t(y)*60+x]=frame?uint8_t(2+(x%40)):127;
  }
 }
 return b;
}
int main(){
 constexpr size_t header=8+2*24;
 auto b=shape();Image first,second;assert(masks::archived(b,0,first));assert(masks::archived(b,1,second));
 assert(first.canvasWidth==60&&first.canvasHeight==30&&first.x==-30&&first.y==-15);
 assert(first.indices[15*60+30]==127&&first.indices[0]==254&&second.indices[15*60+30]==32);
 std::map<uint32_t,std::vector<uint8_t>> memory;
 memory[0x21000]=b;memory[0x20000].resize(16);set16(memory[0x20000],0,65535);set32(memory[0x20000],12,0x21000);
 auto read=[&](uint32_t p,void*out,size_t n){auto i=memory.upper_bound(p);if(i==memory.begin())return false;--i;
  if(uint64_t(p)-i->first+n>i->second.size())return false;memcpy(out,i->second.data()+p-i->first,n);return true;};
 Image live;assert(masks::loaded(read,0x20000,1,live));assert(live.indices==second.indices);
 assert(!masks::loaded(read,0x20000,47,live)&&!masks::archived(b,-1,live));
 auto wrong=shape();set16(wrong,2,61);assert(!masks::archived(wrong,1,live));
 auto invalid=shape();invalid[header+15*60+30]=0;assert(!masks::archived(invalid,0,live));
 assert(!masks::archived({1,2,3},0,live));
 std::cout<<"PASS shroud native SHP 60x30 mask indices, clear/edge/sentinel, live-reference and archive decoding, malformed source rejection\n";
}
