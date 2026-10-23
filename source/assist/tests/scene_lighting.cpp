#include "../../external_reader/source/scene_lighting.h"
#include <vector>
#include <cassert>
#include <iostream>
using namespace external::lighting;
int main(){
 std::vector<uint8_t> memory(0xC00000);auto read=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;};
 auto put=[&](uint32_t p,auto value){memcpy(memory.data()+p,&value,sizeof value);};
 assert(sample(read)==State{});put(0xA8B230,0x10000u);put(0x1352C,100);
 for(int at:{0x13534,0x13538,0x1353C})put(at,100);
 assert(sample(read)==State{});put(0x1352C,75);put(0x13538,80);auto s=sample(read);assert(s.ambient==75&&s.tint[1]==800);
 assert(pixel(0x7C646464,s)==0x7C4B3C4Bu);assert(pixel(0,s)==0);
 put(0xB054D4,0x20000u);put(0xB054E0,2);put(0x20000,0x21000u);put(0x20004,0x22000u);
 put(0x21310,1);put(0x22310,53);put(0x2230C,0x23000u);
 for(int at:{0x23198,0x2319C,0x231A0})put(at,1000);
 put(0x231A4,1200);put(0x231A8,500);put(0x231AC,800);put(0x231B0,uint8_t(1));
 s=sample(read);assert((s.tint==std::array<int,3>({1200,500,800})));assert(pixel(0x7C646464,s)==0x7C5A253Cu);
 put(0x231B0,uint8_t(0));assert(sample(read).tint==State{}.tint);
 put(0x1352C,200);s=sample(read);assert(pixel(0x7C646464,s)==0x7C7C7C7Cu); // clamp RGB to alpha
 put(0x1352C,0);assert(pixel(0xFF808080,sample(read))==0xFF000000u); // intentional black scene
 put(0x1352C,100);put(0x23198,9000);assert(sample(read).tint[1]==800); // invalid converter falls back to scenario
 auto race=[&](uint32_t p,void*out,size_t n){bool ok=read(p,out,n);if(p==0xA8B230)put(0xA8B230,0x30000u);return ok;};assert(sample(race)==State{});
 put(0xA8B230,0xfffffff0u);assert(sample(read)==State{});
 std::cout<<"PASS ambient, normal/tinted converter, map changes, bounded reads, palette-independent RGB, premultiplied alpha and neutral fallback\n";
}
