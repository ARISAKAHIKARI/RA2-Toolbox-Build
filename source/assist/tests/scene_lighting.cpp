#include "../../external_reader/source/scene_lighting.h"
#include "../../external_reader/source/projection.h"
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
 // Cell values already include ambient, Ground, Level and intensity scale.
 State fallback;fallback.ambient=50;constexpr uint32_t c=0x40000;
 assert(cell(read,c,fallback)==fallback);put(c+0x104,65536u);put(c+0x34,0x23000u);
 put(0x23198,1000);put(0x2319C,800);put(0x231A0,1200);
 put(c+0x10A,int16_t(1100));put(c+0x10C,int16_t(825));put(c+0x10E,int16_t(1350));
 auto object=cell(read,c,fallback),terrain=cell(read,c,fallback,Layer::Terrain),bridge=cell(read,c,fallback,Layer::Bridge);
 assert(object.gains()[0]==1.1f&&terrain.gains()[0]==.825f&&bridge.gains()[0]==1.35f);
 assert(object.tint[1]==800&&cell(read,c,fallback,Layer::Object,false).tint[1]==1000);
 put(c+0x10A,int16_t(0));assert(pixel(0xFF808080,cell(read,c,fallback))==0xFF000000);
 put(c+0x10A,int16_t(3000));assert(cell(read,c,fallback)==fallback);put(c+0x10A,int16_t(1100));
 int reads=0;auto cellRace=[&](uint32_t p,void*out,size_t n){bool ok=read(p,out,n);if(p==c+0x10A&&++reads==1)put(c+0x10C,int16_t(900));return ok;};assert(cell(cellRace,c,fallback)==fallback);
 put(0x87F7E8+0x13C,0x100000u);put(0x87F7E8+0x140,512*512);put(0x100000+(3*512+2)*4,c);
 put(c+0x24,int16_t(2));put(c+0x26,int16_t(3));put(c+0x140,0x80u);put(c+0x11B,int8_t(2));put(0x89E7C0,104);
 assert(at(read,640,896,208,fallback).intensity==1100&&at(read,640,896,624,fallback).intensity==1350);
 assert(at(read,640,896,624,fallback,true).intensity==900);assert(cellAt(read,-1,896)==0);put(c+0x26,int16_t(4));assert(cellAt(read,640,896)==0);
 put(0xB0CD48,.144337567);assert(external::projectionZScale(read)==.144337567);
 assert(external::screenHeight(104,external::projectionZScale(read))==15&&external::screenHeight(728,external::projectionZScale(read))==106);
 put(0xB0CD48,0.);assert(external::projectionZScale(read)>0);put(0xB0CD48,10.);assert(external::projectionZScale(read)<1);
 std::cout<<"PASS ambient, normal/tinted converter, map changes, bounded reads, palette-independent RGB, premultiplied alpha and neutral fallback\n";
}
