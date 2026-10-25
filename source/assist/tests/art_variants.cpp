#include "../../external_reader/source/art_variants.h"
#include "../../external_reader/source/watch_view.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <set>
using namespace external;
struct Fixture {
 std::vector<uint8_t> mem=std::vector<uint8_t>(0xC00000);uint32_t fail=0;
 static constexpr uint32_t Overlay=0x20000,Tile=0x50000,Shape=0x60000,Frame=0x61000,Resource=0x18000;
 std::vector<uint32_t> types;
 template<class T>void put(uint32_t p,T v){memcpy(mem.data()+p,&v,sizeof v);}
 bool operator()(uint32_t p,void*out,size_t n){if(p==fail||uint64_t(p)+n>mem.size())return false;memcpy(out,mem.data()+p,n);return true;}
 Fixture(){
  for(int i=0;i<128;++i){uint32_t p=Overlay+i*0x400;types.push_back(p);put(p+0x294,i);put(p+0x2A9,uint8_t(1));}
  put(0xB0F4EC,0x10000u);put(0xB0F4F0,1);put(0xB0F4F8,1);put(0x10000,Resource);
  for(int i=0;i<8;++i){put(Tile+i*0x1000+0x2BC,Tile+uint32_t(i+1)*0x1000);put(Tile+i*0x1000+0xA4,Shape);}
  for(int i=0;i<16;++i)put(0x81CCA8+i*4,((i*3)^(i>>1))&3);
  for(int i=0;i<64;++i)put(0x89E620+i*4,(i*5+(i>>3))&7);
  put(0x89E7C7,uint8_t(1));
 }
 int overlay(int first,int flat,int slopes,int stored,int x,int y,int slope){
  put(Resource+0xE0,types[first]);put(Resource+0xE8,flat);put(Resource+0xEC,slopes);
  auto read=[&](uint32_t p,void*out,size_t n){return (*this)(p,out,n);};
  return artvariants::overlay(read,types,artvariants::pools(read,types),stored,x,y,unsigned(slope));
 }
 int tile(int count,int sub,int x,int y,int flags,int broken,int width,int height){
  put(Tile+0x2F0,count);put(Shape,width);put(Shape+4,height);
  for(int i=0;i<width*height;++i){put(Shape+16+i*4,Frame+uint32_t(i)*0x100);put(Frame+i*0x100+36,broken?4:0);}
  auto read=[&](uint32_t p,void*out,size_t n){return (*this)(p,out,n);};
  return int((artvariants::tile(read,Tile,unsigned(sub),x,y,uint32_t(flags))-Tile)/0x1000);
 }
};
int main(int argc,char**argv){
 Fixture f;
 if(argc==2&&std::string(argv[1])=="--probe"){
  std::string line;while(std::getline(std::cin,line)){std::istringstream s(line);char kind;s>>kind;
   int a,b,c,d,e,g,h,i;if(kind=='O'){s>>a>>b>>c>>d>>e>>g>>h;std::cout<<f.overlay(a,b,c,d,e,g,h)<<'\n';}
   else{s>>a>>b>>c>>d>>e>>g>>h>>i;std::cout<<f.tile(a,b,c,d,e,g,h,i)<<'\n';}
  }return 0;
 }
 std::set<int> ore;
 for(int y=1;y<24;++y)for(int x=1;x<24;++x){int v=f.overlay(10,12,8,10,x,y,0);assert(v==10+x*y%12);ore.insert(v);
  for(int slope=1;slope<=4;++slope)assert(f.overlay(10,12,8,10,x,y,slope)==22+x*y%2+(slope-1)*2);
 }
 assert(ore.size()==12&&"one stored resource type must display every image in its flat pool");
 assert(f.overlay(10,12,0,10,7,9,0)==13);assert(f.overlay(10,12,0,10,7,9,1)==10);
 assert(f.overlay(10,0,8,10,7,9,0)==10);assert(f.overlay(10,12,8,10,7,9,255)==10);
 assert(f.overlay(10,12,8,10,-1,9,0)==10);assert(f.overlay(10,12,8,60,7,9,0)==60);
 f.fail=Fixture::Resource+0xE8;assert(f.overlay(10,12,8,10,7,9,0)==10);f.fail=0;
 f.put(f.types[10]+0x2A9,uint8_t(0));assert(f.overlay(10,12,8,10,7,9,0)==10);f.put(f.types[10]+0x2A9,uint8_t(1));
 std::set<int> clear;
 for(int y=0;y<16;++y)for(int x=0;x<16;++x){int v=f.tile(8,0,x,y,0,0,1,1);assert(v==((x&7)*5+(y&7)*40+(y&7))%8);clear.insert(v);
  assert(f.tile(4,0,x,y,0,0,1,1)==((((y&3)*4+(x&3))*3)^(((y&3)*4+(x&3))>>1))%4);
  assert(f.tile(2,0,x,y,0,1,1,1)==0);assert(f.tile(2,0,x,y,0x2000,1,1,1)==1);
 }
 assert(clear.size()==8);int root=f.tile(8,3,15,17,0,0,2,2);assert(root==f.tile(8,0,7,8,0,0,1,1));
 f.fail=0x89E620;assert(f.tile(8,0,0,0,0,0,1,1)==0);f.fail=0;
 f.put(0x89E7C7,uint8_t(0));assert(f.tile(8,0,7,3,0,0,1,1)==0);f.put(0x89E7C7,uint8_t(1));
 f.put(Fixture::Tile+0x2BC,0u);assert(f.tile(8,0,1,0,0,0,1,1)==0);
 assert(f.tile(0,0,1,0,0,0,1,1)==0);assert(f.tile(65,0,1,0,0,0,1,1)==0);
 // Exercise both real production samplers: neighboring cells have identical
 // stored map indices/data but select different resource SHPs and TMP types.
 {
  Fixture s;constexpr uint32_t Cells=0x100000,Cell=0x80000,Object=0x70000;
  s.overlay(0,12,8,0,40,42,0);s.tile(8,0,40,42,0,0,1,1);
  s.put(0xA8ED2C,0x14000u);s.put(0xA8ED30,1);s.put(0xA8ED38,1);s.put(0x14000,Fixture::Tile);
  s.put(0xA83D84,0x15000u);s.put(0xA83D88,20);s.put(0xA83D90,20);for(int n=0;n<20;++n)s.put(0x15000+n*4,s.types[n]);
  s.put(radar::Map+0xF4,64);s.put(radar::Map+0xF8,64);s.put(radar::Map+0x104,64);s.put(radar::Map+0x108,64);
  s.put(radar::Map+0x13C,Cells);s.put(radar::Map+0x140,32768);s.put(radar::Map+0x1488,1.f);s.put(radar::Map+0x1490,64);s.put(radar::Map+0x1498,64);s.put(radar::Map+0x14A4,128);s.put(radar::Map+0x14A8,128);s.put(radar::Map+0x1240,128);s.put(radar::Map+0x1244,128);
  s.put(0xA83D4C,0x90000u);s.put(0xA8ED84,1);s.put(0xA8E9A0,uint8_t(1));s.put(0xA8E378,uint8_t(1));
  s.put(Object,0x7F5C70u);s.put(Object+0x10,1u);s.put(Object+0x6C,100);s.put(Object+0x74,uint8_t(1));s.put(Object+0x90,uint8_t(1));s.put(Object+0x9C,40*256+128);s.put(Object+0xA0,42*256+128);
  for(int n=0;n<2;++n){auto p=Cell+n*0x200;s.put(Cells+(42*512+40+n)*4,p);s.put(p+0x24,int16_t(40+n));s.put(p+0x26,int16_t(42));s.put(p+0x120,int8_t(-1));s.put(p+0x121,int8_t(-1));s.put(p+0x11E,uint8_t(7));}
  auto read=[&](uint32_t p,void*out,size_t n){return s(p,out,n);};
  auto w=watch::sample(read,{Object,1},true);assert(w.focus);
  for(int n=0;n<2;++n){const auto&t=w.tiles[(42-w.cy+w.radius)*w.side+40+n-w.cx+w.radius];
   assert(t.overlayType==s.types[(40+n)*42%12]&&t.overlayFrame==7);
   assert(t.type==Fixture::Tile+uint32_t(s.tile(8,0,40+n,42,0,0,1,1))*0x1000);
  }
  radar::Sampler sampler(read);sampler.step(1,16384,true);sampler.step(1,16384,true);auto r=sampler.snapshot(1);assert(r.terrain);
  for(int n=0;n<2;++n){const auto&a=r.terrain->art[42*512+40+n];const auto&t=w.tiles[(42-w.cy+w.radius)*w.side+40+n-w.cx+w.radius];
   assert(a.overlay==0&&a.overlaySourceType==s.types[0]&&a.overlayType==t.overlayType&&a.overlayFrame==7&&a.type==t.type);
  }
 }
 std::cout<<"PASS resource image pools, flat/slope choices, native TMP coordinate tables and linked/damaged variants, stable selection and unreadable/corrupt fallbacks\n";
}
