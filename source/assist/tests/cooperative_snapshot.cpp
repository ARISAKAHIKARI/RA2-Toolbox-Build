#ifndef COOPERATIVE_READER
#define COOPERATIVE_READER "../../external_reader/source/cooperative_reader.h"
#endif
#include COOPERATIVE_READER
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
int main(int argc,char**){
 using namespace assist;std::vector<uint8_t> memory(0xC00000);std::map<uint32_t,unsigned> coordinates;
 auto put=[&](uint32_t p,auto v){std::memcpy(memory.data()+p,&v,sizeof v);};
 auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;if(n==4&&p>=0x100000&&p<0x300000&&(p&0x7ff)==0x9c)++coordinates[p];std::memcpy(out,memory.data()+p,n);return true;};
 constexpr uint32_t house=0x10000,tt=0x50000,bt=0x52000,unit=0x60000,pointers=0x90000;
 put(0xA83D4C,house);put(0xA8ED84,100);put(0xA8E9A0,uint8_t(1));put(tt+0x380,3.);put(tt+0xD2E,uint8_t(1));put(bt+0x16AB,uint8_t(1));
 auto object=[&](uint32_t p,uint32_t vt,uint32_t type,uint32_t id){put(p,vt);put(p+0x10,id);put(p+(vt==0x7e3ebc?0x520:0x6c4),type);put(p+0x21c,house);put(p+0x6c,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9c,25600);put(p+0xa0,25600);};
 object(unit,0x7F5C70,tt,7);put(pointers,unit);
 for(unsigned n=0;n<1000;++n){uint32_t p=0x100000+n*0x800;object(p,0x7E3EBC,bt,1000+n);put(pointers+(n+1)*4,p);}
 put(0xA8EC78+4,pointers);put(0xA8EC78+8,1001);put(0xA8EC78+16,1001);
 cooperative::Request request{};request.serial=request.match=1;request.target=1999;request.type=bt;request.mode=cooperative::Enter;request.count=1;request.sources[0]={7,0};request.cameraX=-320;request.cameraY=2700;request.viewWidth=640;request.viewHeight=480;
 auto input=external::cooperativeInput(read,request,[](uint32_t){return cooperative::Quota{};});assert(input.units.size()==1&&input.targets.size()==512&&input.targets[0].id==1999);
 for(size_t n=1;n<input.targets.size();++n)assert(input.targets[n].id==999+n);
 unsigned maximum=0,total=0;for(auto c:coordinates){maximum=std::max(maximum,c.second);total+=c.second;}
 std::cout<<"1000 candidates: x-coordinate reads="<<total<<", maximum per candidate="<<maximum<<"; clicked-first and ID tie order preserved\n";
 // The optional benchmark switch measures the old reader without asserting
 // its read complexity; normal regression requires fixed per-object work.
 if(argc==1)assert(maximum<=5&&total<=5000);
 auto assignments=cooperative::allocate(input);assert(assignments.size()==1&&assignments[0].target==1999);
}
