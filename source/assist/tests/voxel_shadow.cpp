#include "../../external_reader/source/visual_pose.h"
#include "../../external_reader/source/gpu_voxel.h"
#include <cassert>
#include <iostream>
#include <iomanip>
using namespace external;
namespace {
constexpr uint32_t Object=0x10000,Loco=0x20000,Cells=0x30000,Cell=0x40000;
std::vector<uint8_t> memory(0xC00000);uint32_t failed=0;
struct Read {bool operator()(uint32_t p,void*out,size_t n)const{if(p==failed||uint64_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;}};
template<class T>void put(uint32_t p,const T&v){std::memcpy(memory.data()+p,&v,sizeof v);}
void setup(bool ship=false){
 std::fill(memory.begin(),memory.end(),0);put(Object+0x674,Loco);put(Loco,ship?uint32_t(0x7F2D8C):uint32_t(0x7E7EB0));put(Loco+8,Object);
 put(0x87F924,Cells);put(0x87F928,512*512);put(Cells+(30*512+20)*4,Cell);put(Cell+0x24,std::array<int16_t,2>{{20,30}});
 put(Object+0x9C,std::array<int,3>{{20*256+128,30*256+128,52}});put(Cell+0x11C,uint8_t(1));
 put(0x89E7C0,104);put(0x89E770,uint8_t(7));put(0x89E740,104./256);put(0xB0CD48,.14433756729740643);put(0xAC13BC,416);
 put(0x81C900,std::array<double,5>{{1,0,0,104,0}});
 auto matrix=voxelposture::identity();matrix[0]=matrix[10]=.96f;matrix[2]=.28f;matrix[8]=-.28f;put(0xB45188+48,matrix);
 put(0xB45188,voxelposture::identity());
}
}
int main(int argc,char**){
 std::cout<<std::setprecision(10);
 if(argc>1){char command;while(std::cin>>command){
  if(command=='F'){int level,height,x,y,result;unsigned ramp;double scale;std::array<double,5>terms{};std::cin>>level>>height>>x>>y>>ramp>>scale;for(auto&v:terms)std::cin>>v;
   assert(voxelshadow::floorHeight(level,height,x,y,ramp,scale,terms,result));std::cout<<result<<'\n';}
  else{assert(command=='M');double angle;voxelposture::Matrix ramp{};std::cin>>angle;for(auto&v:ramp)std::cin>>v;auto matrix=voxelposture::multiply(ramp,voxelposture::yaw(-angle,true));for(float v:matrix)std::cout<<v<<' ';std::cout<<'\n';}
 }return 0;}
 GameView<Read> game(Read{});
 for(bool ship:{false,true}){
  setup(ship);auto shadow=voxelshadow::capture(game,Object,2);assert(shadow.known&&shadow.posture.available&&shadow.posture.active&&shadow.drop==0);
  // The floor follows the exact XY within the slope; Cell Level remains zero.
  put(Object+0x9C,std::array<int,3>{{20*256+192,30*256+128,78}});shadow=voxelshadow::capture(game,Object,2);assert(shadow.drop==0);
  put(Object+0xA4,130);shadow=voxelshadow::capture(game,Object,2);assert(shadow.drop==8);
  put(Object+0x8C,uint8_t(1));put(Object+0xA4,494);shadow=voxelshadow::capture(game,Object,2);assert(shadow.drop==0);
  // Changing animated hull rocking and ramp transition never changes shadow.
  auto observed=shadow;put(Object+0x328,.8f);put(Loco+0x18,std::array<int,6>{{2,9,100,0,8,8}});shadow=voxelshadow::capture(game,Object,2);assert(shadow.posture.ramp==observed.posture.ramp&&shadow.drop==observed.drop);
  auto projected=voxelshadow::projection(shadow);auto point=voxel::posturePoint(projected.ramp,{10,4,0});assert(point.z==0&&point.x!=10);
  visual::Pose a,b;a.shadow=shadow;b=a;b.shadow.posture.ramp=voxelposture::identity();b.shadow.posture.active=false;assert(visual::postureSignature(a)!=visual::postureSignature(b));
  b=a;++b.shadow.drop;assert(visual::postureSignature(a)==visual::postureSignature(b)); // drop is a draw coordinate, not a new geometry cache owner
  a.posture=a.shadow.posture;b=a;b.posture.ramp[0]+=.01f;b.shadow.posture=b.posture;assert(visual::postureSignature(a)!=visual::postureSignature(b)); // equal hull/shadow slopes must not cancel each other in a cache key
  failed=Cell+0x11C;auto bad=voxelshadow::capture(game,Object,2);assert(bad.known&&!bad.posture.available);failed=0;
  put(Cell+0x11C,uint8_t(21));assert(!voxelshadow::capture(game,Object,2).posture.available);
  put(Loco,uint32_t(123));assert(!voxelshadow::capture(game,Object,2).known);
 }
 std::cout<<"PASS Drive/Ship independent Cell ramp shadow, exact within-cell floor and bridge drop, no hull rocking/timer, world-plane projection, geometry-cache identity and invalid-read isolation\n";
}
