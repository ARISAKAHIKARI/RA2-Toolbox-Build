#pragma once
#include "visual_pose.h"
#include "voxel_preview.h"
#include "map_bounds.h"
namespace external {namespace voxelanims {
constexpr int Kind=8;constexpr uint32_t Array=0x887388,Vtable=0x7F6318;
struct State {uint32_t object=0,id=0,type=0,owner=0;int x=0,y=0,z=0,layer=3,section=0;visual::Pose pose;};
template<class Read>bool capture(Read read,uint32_t p,State&out){
 GameView game(read);std::array<uint8_t,0x148>d{},again{};if(p<0x10000||!read(p,d.data(),d.size())||voxel::u32(d.data())!=Vtable||d[0x81]||!d[0x90]||d[0x110]||d[0x13C])return false;
 State s;s.object=p;s.id=voxel::u32(d.data()+0x10);s.type=voxel::u32(d.data()+0x104);s.owner=voxel::u32(d.data()+0x10C);if(!s.id||s.type<0x10000)return false;std::memcpy(&s.x,d.data()+0x9C,12);if(s.x<0||s.y<0||s.x>=512*256||s.y>=512*256||std::abs(int64_t(s.z))>131072)return false;
 std::array<float,4>q{};std::memcpy(q.data(),d.data()+0xE0,16);if(!voxelposture::validQuaternion(q))return false;double x=q[0],y=q[1],z=q[2],w=q[3];s.pose.body=s.pose.turret=8;s.pose.voxel=true;s.pose.posture.active=true;s.pose.posture.ramp={{float(1-2*(y*y+z*z)),float(2*(x*y-w*z)),float(2*(x*z+w*y)),0,float(2*(x*y+w*z)),float(1-2*(x*x+z*z)),float(2*(y*z-w*x)),0,float(2*(x*z-w*y)),float(2*(y*z+w*x)),float(1-2*(x*x+y*y)),0}};
 s.section=game.template get<int>(s.type+0x298,-1);if(s.section<0||s.section>=32)return false;
 // Native debris uses the current floor and a bridge deck at/under its Z.
 uint32_t cells=game.template get<uint32_t>(mapbounds::Map+0x13C),cell=0;int count=game.template get<int>(mapbounds::Map+0x140),index=s.y/256*512+s.x/256;
 if(cells>=0x10000&&count>index&&count<=512*512&&read(cells+index*4,&cell,4)&&cell>=0x10000){int height=game.template get<int>(0x89E7C0,104),floor=0;int8_t level=game.template get<int8_t>(cell+0x11B);uint8_t ramp=game.template get<uint8_t>(cell+0x11C);std::array<double,5>terms{};double scale=1;if(!ramp||(read(0x81C900+(ramp-1)*40,terms.data(),40)&&read(0x89E740,&scale,8))){if(voxelshadow::floorHeight(level,height,s.x,s.y,ramp,scale,terms,floor)){int bridge=game.template get<int>(0xAC13BC,416);if((game.template get<uint32_t>(cell+0x140)&0x100)&&s.z>=floor+bridge)floor+=bridge;s.pose.shadow.known=true;s.pose.shadow.drop=screenHeight(s.z-floor,projectionZScale(read));}}}
 if(!read(p,again.data(),again.size())||std::memcmp(d.data(),again.data(),0x114))return false;out=s;return true;
}
template<class Read>std::vector<State>all(Read read){GameView game(read);std::vector<State>out;for(auto p:game.vec(Array,65536)){State s;if(capture(read,p,s))out.push_back(s);}return out;}
template<class A,class Art>auto captureArtImpl(A&a,const State&s,Art&out,int)->decltype(a.captureVoxelAnimation(s.object,s.type,s.id,out),bool()){return a.captureVoxelAnimation(s.object,s.type,s.id,out);}
template<class A,class Art>bool captureArtImpl(A&,const State&,Art&,long){return false;}
template<class A,class Art>bool captureArt(A&a,uint32_t p,uint32_t type,uint32_t id,Art&out){State s;s.object=p;s.type=type;s.id=id;return captureArtImpl(a,s,out,0);}
}}
