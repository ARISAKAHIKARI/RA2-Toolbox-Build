#include "../../external_reader/source/visual_pose.h"
#include "../../external_reader/source/voxel_preview.h"
#include <cassert>
#include <cstdio>
#include <iomanip>
#include <iostream>
using namespace external;
namespace {
constexpr uint32_t Object=0x10000,Type=0x20000,Loco=0x30000;
struct Memory {
 std::vector<uint8_t> data=std::vector<uint8_t>(0xC00000);uint32_t fail=0;bool race=false;int stateReads=0;
 bool operator()(uint32_t at,void*out,size_t n){
  if(at==fail||uint64_t(at)+n>data.size())return false;
  std::memcpy(out,data.data()+at,n);if(race&&at==Loco+0x18&&++stateReads==2)static_cast<int*>(out)[0]++;return true;
 }
 template<class T>void put(uint32_t at,const T&v){std::memcpy(data.data()+at,&v,sizeof v);}
}memory;
struct Read {bool operator()(uint32_t at,void*out,size_t n)const{return memory(at,out,n);}};
void setup(int current=0,int previous=0,int rate=0,int elapsed=0,float side=0,float forward=0,int bucket=8,bool ship=false){
 memory.put(Object,uint32_t(0x7F5C70));memory.put(Object+0x674,Loco);memory.put(Type+0xB0,uint32_t(1));
 memory.put(Loco,ship?uint32_t(0x7F2D8C):uint32_t(0x7E7EB0));memory.put(Loco+8,Object);
 const int state[]{current,previous,100,0,rate,rate};memory.put(Loco+0x18,state);
 const float rock[]{side,forward};memory.put(Object+0x328,rock);const double scale[]{1,1};memory.put(Type+0x360,scale);
 memory.put(Object+0x388,uint16_t(bucket*2048));memory.put(Object+0x38C,uint16_t(bucket*2048));memory.put(Object+0x390,-1);
 memory.put(Object+0x398,0);memory.put(Object+0x39C,int16_t(0));memory.put(0xA8ED84,100+elapsed);
}
voxelposture::Matrix ramp(float angle){float c=std::cos(angle),s=std::sin(angle);return {{c,0,s,0,0,1,0,0,-s,0,c,0}};}
voxel::Model model(){
 voxel::Model out;const voxel::Vec points[]={{-12,-5,0},{12,-5,0},{12,5,0},{-12,5,0},{-12,-5,5},{12,-5,5},{12,5,5},{-12,5,5}};
 const int indices[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};
 for(int n=0;n<6;++n){voxel::Face f;for(int i=0;i<4;++i)f.p[i]=points[indices[n][i]];f.color=32+n;f.normalType=4;f.normalIndex=1;f.hasNormal=true;f.normal={0,0,1};out.faces.push_back(f);}
 return out;
}
}
int main(int argc,char**){
 GameView<Read> game(Read{});
 if(argc>1){
  int current,previous,start,duration,rate,frame,bucket,ship;float side,forward;double scaleX,scaleY;
  std::cout<<std::setprecision(10);
  while(std::cin>>current>>previous>>start>>duration>>rate>>frame>>bucket>>ship>>side>>forward>>scaleX>>scaleY){
   setup(current,previous,rate,0,side,forward,bucket,ship!=0);
   const int state[]{current,previous,start,0,duration,rate};memory.put(Loco+0x18,state);const double scale[]{scaleX,scaleY};memory.put(Type+0x360,scale);
   voxelposture::Matrix a{},b{};std::array<float,4>qa{},qb{};
   for(auto&v:a)std::cin>>v;for(auto&v:b)std::cin>>v;for(auto&v:qa)std::cin>>v;for(auto&v:qb)std::cin>>v;
   memory.put(0xB45188+current*48,a);memory.put(0xB45188+previous*48,b);memory.put(0xB43188+current*16,qa);memory.put(0xB43188+previous*16,qb);
   auto pose=voxelposture::capture(game,Object,Type,2,frame);double angle=visual::radians(bucket);
   auto matrix=voxelposture::multiply(voxelposture::world(pose,angle),voxelposture::yaw(-angle,true));
   std::cout<<pose.available;for(float v:matrix)std::cout<<' '<<v;std::cout<<'\n';
  }return 0;
 }
 auto identity=voxelposture::identity();auto incline=ramp(.28f);memory.put(0xB45188,identity);memory.put(0xB45188+48,incline);
 const std::array<float,4>q0{{0,0,0,1}},q1{{0,std::sin(.14f),0,std::cos(.14f)}};
 memory.put(0xB43188,q0);memory.put(0xB43188+16,q1);
 setup();auto flat=visual::pose(game,Object,Type,2,100);assert(flat.voxel&&flat.posture.available&&!flat.posture.active);
 // A cell level/height value is not a slope rotation and never replaces the
 // locomotor's live draw state. No terrain metadata is read by this capture.
 memory.put(Object+0xA4,4096);assert(!visual::pose(game,Object,Type,2,100).posture.active);
 setup(1);auto sloped=visual::pose(game,Object,Type,2,100);assert(sloped.posture.available&&sloped.posture.active&&sloped.posture.ramp==incline);
 setup(1,0,8,4);auto halfway=visual::pose(game,Object,Type,2,104);assert(halfway.posture.available&&halfway.posture.active);
 auto endPoint=voxel::posturePoint(incline,{10,0,0}),midPoint=voxel::posturePoint(halfway.posture.ramp,{10,0,0});
 assert(endPoint.z<midPoint.z&&midPoint.z<0&&midPoint.x>endPoint.x);
 setup(1,0,0,0,.1f,.15f);auto rocking=visual::pose(game,Object,Type,2,100);assert(rocking.posture.available&&rocking.posture.active);
 assert(voxelposture::signature(flat.posture)==0&&voxelposture::signature(sloped.posture)!=voxelposture::signature(halfway.posture)&&voxelposture::signature(sloped.posture)!=voxelposture::signature(rocking.posture));
 auto body=model();auto stored=body.faces.data();std::array<uint32_t,256> colors{};for(unsigned i=0;i<256;++i)colors[i]=0xff000000u|i*0x010101u;
 voxel::Image normal,tilted,rocked;assert(voxel::render(body,0,colors,normal,0,3,nullptr,255,0,false,1,&flat.posture));
 assert(voxel::render(body,0,colors,tilted,0,3,nullptr,255,0,false,1,&sloped.posture));
 assert(voxel::render(body,0,colors,rocked,0,3,nullptr,255,0,false,1,&rocking.posture));
 assert(normal.pixels!=tilted.pixels&&tilted.pixels!=rocked.pixels&&body.faces.data()==stored&&body.faces.size()==6);
 auto light=voxel::postureLight(0,voxelposture::world(sloped.posture,0));assert(light.source!=voxel::nativeVoxelLight(0).source);
 for(uint32_t p:tilted.pixels)assert(!p||p>>24==255);
 setup(1);memory.race=true;memory.stateReads=0;auto race=visual::pose(game,Object,Type,2,100);assert(!race.posture.available);memory.race=false;
 assert(!voxel::render(body,0,colors,tilted,0,3,nullptr,255,0,false,1,&race.posture));
 memory.put(0xB45188+48,voxelposture::Matrix{});assert(!visual::pose(game,Object,Type,2,100).posture.available);memory.put(0xB45188+48,incline);
 memory.fail=Loco+0x18;assert(!visual::pose(game,Object,Type,2,100).posture.available);memory.fail=0;
 setup(1);memory.put(Loco,uint32_t(0x12345678));auto custom=visual::pose(game,Object,Type,2,100);assert(custom.posture.available&&!custom.posture.active);
 setup(1,0,0,0,0,0,8,true);auto naval=visual::pose(game,Object,Type,2,100);assert(naval.posture.available&&naval.posture.ramp==incline);
 std::puts("PASS: Drive/Ship live ramp and transition, rocking, real vertex/normal projection, opaque output, retained model geometry, race/table failure rejection and custom-locomotor isolation");
}
