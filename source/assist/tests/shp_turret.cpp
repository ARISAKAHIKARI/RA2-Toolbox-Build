#include "../../external_reader/source/visual_pose.h"
#include <cassert>
#include <iostream>
using namespace external;
int main(int argc,char**){
 if(argc>1){unsigned raw;int walk;while(std::cin>>raw>>walk)std::cout<<visual::shpTurretFrame(uint16_t(raw),walk)<<'\n';return 0;}
 for(int walk:{0,1,2,7,31,127})for(unsigned direction=0;direction<32;++direction){
  assert(visual::shpTurretFrame(uint16_t(direction*2048),walk)==walk*8+int((direction+4)&31));
 }
 assert(visual::shpTurretFrame(0,-1)==-1&&visual::shpTurretFrame(0,128)==-1);
 std::vector<uint8_t> memory(0x200000);auto read=[&](uint32_t at,void*out,size_t n){if(uint64_t(at)+n>memory.size())return false;std::memcpy(out,memory.data()+at,n);return true;};
 auto put=[&](uint32_t at,const auto&v){std::memcpy(memory.data()+at,&v,sizeof v);};
 constexpr uint32_t object=0x10000,type=0x20000;put(type+0xE3C,8);put(type+0xE5C,int8_t(3));put(type+0xCA1,uint8_t(1));put(object+0x6C0,-1);
 put(type+0xE1C,1);put(type+0xE28,32);put(object+0x388,uint16_t(0));put(object+0x38C,uint16_t(0));put(object+0x390,-1);
 put(object+0x3A0,uint16_t(0));put(object+0x3A4,uint16_t(0));put(object+0x3A8,-1);GameView game(read);
 auto a=visual::pose(game,object,type,2,100);assert(a.frame==33&&a.turretFrame==28);
 put(object+0x3A0,uint16_t(16*2048));put(object+0x3A4,uint16_t(16*2048));auto b=visual::pose(game,object,type,2,100);assert(b.frame==a.frame&&b.turretFrame==44);
 put(type+0xCA1,uint8_t(0));assert(visual::pose(game,object,type,2,100).turretFrame==-1);
 put(type+0xCA1,uint8_t(1));put(type+0xB0,uint32_t(1));assert(visual::pose(game,object,type,2,100).turretFrame==-1);
 std::cout<<"PASS same-SHP 32-facing turret bank, WalkFrames offset, independent live facing, no-turret/VXL isolation and invalid bank rejection\n";
}
