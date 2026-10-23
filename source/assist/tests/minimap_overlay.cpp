#include <cassert>
#include <iostream>
#include <map>
#include <cstring>
#include "../../external_reader/source/radar_map.h"
#include "../source/radar_layout.h"
#include "../../external_reader/source/radar_navigation.h"
struct Memory {
 std::map<uint32_t,uint8_t> bytes;
 bool operator()(uint32_t at,void*out,size_t n)const{auto*p=(uint8_t*)out;for(size_t i=0;i<n;++i){auto it=bytes.find(at+i);if(it==bytes.end())return false;p[i]=it->second;}return true;}
 template<class T>void set(uint32_t at,T v){auto*p=(uint8_t*)&v;for(size_t i=0;i<sizeof v;++i)bytes[at+i]=p[i];}
 void zero(uint32_t at,int n){for(int i=0;i<n;++i)bytes[at+i]=0;}
};
struct Read{Memory*m;bool operator()(uint32_t p,void*out,size_t n)const{return (*m)(p,out,n);}};
int main(){using namespace external::radar;Memory m;constexpr uint32_t cells=0x100000,cell=0x200000,house=0x300000;
 m.set(Map+0xF4,4);m.set(Map+0xF8,4);m.set(Map+0x13C,cells);m.set(Map+0x140,4096);m.set(Map+0x1488,10.f);m.set(Map+0x1490,4);m.set(Map+0x1498,4);m.set(Map+0x14A4,80);m.set(Map+0x14A8,80);
 m.set(0xA83D4C,house);m.set(0xA8ED84,10);m.set<uint8_t>(0xA8E9A0,1);m.set<uint8_t>(0xA8E378,1);m.set(0xA8EDA0,0);m.set(0xA8ED8C,0);
 m.zero(cells,4096*4);m.set(cells+(3*512+3)*4,cell);m.zero(cell,0x148);m.set<int16_t>(cell+0x24,3);m.set<int16_t>(cell+0x26,3);m.set(cell+0xEC,2);m.set<int8_t>(cell+0x120,-1);m.set<int8_t>(cell+0x121,-1);
 Sampler<Read> sampler(Read{&m});assert(sampler.step(false,2048));assert(!sampler.snapshot(1).terrain);assert(sampler.step(false,2048));auto a=sampler.snapshot(2);assert(a.terrain&&a.terrain->colors[3*512+3]==terrainColor(2,0,false));
 double u,v;a.terrain->geo.project(3.5,3.5,u,v);assert(u==.5&&v==.375);std::vector<uint32_t> pixels;raster(*a.terrain,80,80,pixels);assert(pixels[30*80+40]==terrainColor(2,0,false));
 int nx=0,ny=0;assert(navigate(a.terrain->geo,41,31,82,82,120,80,nx,ny));assert(nx==-60&&ny==65);
 assert(navigate(a.terrain->geo,81,61,162,162,120,80,nx,ny)&&nx==-60&&ny==65);
 assert(navigate(a.terrain->geo,-50,999,82,82,120,80,nx,ny)&&nx==-180&&ny==140);
 assert(!navigate(a.terrain->geo,1,1,2,82,120,80,nx,ny));
 // Interpolate known cells without bleeding visible colors into black shroud.
 Terrain mix=*a.terrain;mix.colors[3*512+3]=0x010101;mix.colors[3*512+4]=0xffffff;
 assert(smoothColor(mix,3.5,3.5)==0x010101);assert(smoothColor(mix,4.,3.5)==0x808080);
 mix.colors[3*512+4]=0;assert(smoothColor(mix,4.,3.5)==0x080c08);assert(smoothColor(mix,3.9,3.5)==0x010101);
 // Never expose a partially refreshed terrain bank; old immutable bank survives.
 m.set<int8_t>(cell+0x120,-2);sampler.step(false,2048);assert(sampler.snapshot(3).terrain==a.terrain);sampler.step(false,2048);assert(sampler.snapshot(4).terrain->colors[3*512+3]==0);assert(a.terrain->colors[3*512+3]!=0);
 sampler.step(true,4096);assert(sampler.snapshot(5).terrain->colors[3*512+3]!=0);
 m.set(0xA8ED84,1);sampler.step(true,1);assert(!sampler.snapshot(6).terrain);sampler.step(true,4096);assert(sampler.snapshot(7).terrain);
 m.set(0xA83D4C,house+0x10000);sampler.step(true,1);assert(!sampler.snapshot(8).terrain);m.set(0xA8ED8C,1);assert(!sampler.step(true));assert(!sampler.snapshot(9).terrain);m.set(0xA8ED8C,0);
 m.set(Map+0x1488,std::numeric_limits<float>::quiet_NaN());assert(!sampler.step(true));m.set(Map+0x1488,10.f);m.set(Map+0x140,999999);assert(!sampler.step(true));
 raster(*a.terrain,-1,80,pixels);assert(pixels.empty());int count=0;
 for(int gw:{800,1920,2560,3840})for(int gh:{600,1080,1440,2160})for(int cw:{640,1920,3840})for(int ch:{480,1080,2160})for(int scale:{100,200,400})for(int mode:{0,1,2,3})for(int coord:{0,300,32768})for(bool plane:{false,true}){
  auto aircraft=plane?radarlayout::aircraft(gw,gh,cw,ch):radarlayout::Box{};auto layout=radarlayout::place(gw,gh,cw,ch,scale,mode,coord,coord,aircraft);assert(layout.valid);auto b=layout.destination;
  assert(b.x>=0&&b.y>=0&&b.x+b.w<=cw&&b.y+b.h<=ch);assert(!radarlayout::overlap(b,aircraft)&&!radarlayout::overlap(b,layout.source));++count;
 }
 std::cout<<"PASS: independent map projection/raster, atomic terrain banks, shroud/reveal, new round/house/menu resets, invalid geometry; "<<count<<" layout combinations\n";
}
