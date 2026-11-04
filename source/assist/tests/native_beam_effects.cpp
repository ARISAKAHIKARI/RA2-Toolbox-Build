#include "../../external_reader/source/native_beam_effects.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
using namespace external;
using nativefeedback::Rect;
struct Memory {
 std::map<uint32_t,uint8_t> bytes;unsigned headReads=0;bool mutate=false;
 void put(uint32_t p,const void*d,size_t n){auto b=static_cast<const uint8_t*>(d);for(size_t i=0;i<n;++i)bytes[p+uint32_t(i)]=b[i];}
 template<class T>void put(uint32_t p,T v){put(p,&v,sizeof v);}
 bool read(uint32_t p,void*d,size_t n){
  if(p==nativeeffects::LaserArray&&++headReads==2&&mutate)put(p+16,uint32_t(0));
  auto b=static_cast<uint8_t*>(d);for(size_t i=0;i<n;++i){auto f=bytes.find(p+uint32_t(i));if(f==bytes.end())return false;b[i]=f->second;}return true;
 }
};
static mainview::View view(){mainview::View v;v.scenario=0x10000;v.house=0x20000;v.tactical=0x30000;v.frame=100;v.screenW=640;v.screenH=480;v.width=640;v.height=480;return v;}
static bool contains(const std::vector<Rect>&rows,int x,int y){for(auto r:rows)if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h)return true;return false;}
int main(){
 auto v=view();Memory memory;constexpr uint32_t object=0x40000,pointers=0x41000;
 std::array<uint32_t,6> head{{0,pointers,8,0,1,10}};memory.put(nativeeffects::LaserArray,head.data(),sizeof head);memory.put(pointers,object);memory.put(0xB0CD48,double(15/104.));
 std::array<uint8_t,0x5C> state{};int duration=15,thickness=64;nativefeedback::Point3 source{1536,1536,64},target{3072,2048,64};
 std::memcpy(state.data()+0x24,&source,sizeof source);std::memcpy(state.data()+0x30,&target,sizeof target);std::memcpy(state.data()+0x4C,&duration,4);std::memcpy(state.data()+0x1C,&thickness,4);state[0x20]=1;
 memory.put(object,state.data(),state.size());auto read=[&](uint32_t p,void*d,size_t n){return memory.read(p,d,n);};
 std::vector<Rect> rows;assert(nativeeffects::beams(read,v,640,480,rows)&&!rows.empty());
 assert(contains(rows,60,231)&&!contains(rows,400,100));
 size_t pixels=0;for(auto r:rows){assert(r.valid()&&r.h==1&&r.x>=0&&r.y>=0&&r.x+r.w<=640&&r.y+r.h<=480);pixels+=size_t(r.w)*r.h;}
 assert(pixels<640*480/20); // A thin path, not its full diagonal bounding box.
 const size_t singleCount=rows.size();head[4]=2;memory.put(nativeeffects::LaserArray,head.data(),sizeof head);memory.put(pointers+4,object+0x2000);memory.put(object+0x2000,state.data(),state.size());memory.headReads=0;rows.clear();
 assert(nativeeffects::beams(read,v,640,480,rows)&&rows.size()==singleCount);
 // LaserDraw has no 128-object native limit. More than 128 coincident or
 // inactive effects must keep the same bounded native holes, not reject a
 // complete upgraded scene because of a visual sampler's arbitrary budget.
 head[2]=256;head[4]=129;memory.put(nativeeffects::LaserArray,head.data(),sizeof head);
 for(unsigned i=0;i<129;++i){uint32_t p=0x50000+i*0x80;memory.put(pointers+i*4,p);memory.put(p,state.data(),state.size());}
 memory.headReads=0;rows.clear();assert(nativeeffects::beams(read,v,640,480,rows)&&rows.size()==singleCount);
 for(unsigned i=1;i<129;++i)memory.put(0x50000+i*0x80+0x4C,int(0));
 memory.headReads=0;rows.clear();assert(nativeeffects::beams(read,v,640,480,rows)&&rows.size()==singleCount);
 head[2]=8;memory.put(pointers,object);memory.put(pointers+4,object+0x2000);
 head[4]=1;memory.put(nativeeffects::LaserArray,head.data(),sizeof head);
 state[0x51]=1;memory.put(object,state.data(),state.size());memory.headReads=0;rows.clear();assert(nativeeffects::beams(read,v,640,480,rows)&&rows.empty());
 state[0x51]=0;duration=0;std::memcpy(state.data()+0x4C,&duration,4);memory.put(object,state.data(),state.size());memory.headReads=0;assert(nativeeffects::beams(read,v,640,480,rows)&&rows.empty());
 duration=15;std::memcpy(state.data()+0x4C,&duration,4);memory.put(object,state.data(),state.size());memory.headReads=0;memory.mutate=true;rows={{1,2,3,4}};assert(!nativeeffects::beams(read,v,640,480,rows));assert(rows.size()==1&&rows[0].x==1);
 memory.mutate=false;memory.headReads=0;head[4]=2;memory.put(nativeeffects::LaserArray,head.data(),sizeof head);memory.put(pointers+4,object);assert(!nativeeffects::beams(read,v,640,480,rows));
 // Native-call oracle supplied by native_beam_capture.py: all original line
 // calls, both draw quality branches, all eight headings and supported widths.
 unsigned cases=0;if(std::cin>>cases)for(unsigned i=0;i<cases;++i){
  int x0,y0,x1,y1,radius;unsigned count;std::cin>>x0>>y0>>x1>>y1>>radius>>count;
  struct Line{int a,b,c,d;};std::vector<Line> lines(count);for(auto&l:lines)std::cin>>l.a>>l.b>>l.c>>l.d;
  for(int factor:{1,2,4}){
   rows.clear();assert(nativeeffects::beamCorridor(x0,y0,x1,y1,radius,v,640*factor,480*factor,rows));
   for(const auto&l:lines)for(unsigned step=0;step<=256;++step){
    double t=double(step)/256;int x=int(std::lround((l.a+(l.c-l.a)*t)*factor)),y=int(std::lround((l.b+(l.d-l.b)*t)*factor));
    if(x>=0&&y>=0&&x<640*factor&&y<480*factor)assert(contains(rows,x,y));
   }
  }
 }
 std::cout<<"PASS: native Laser/Prism thin cutouts; native lifetime/flicker, list coherence, bounded coverage; "<<cases<<" original geometry oracle cases\n";
}
