#pragma once
#include "game_view.h"
#include <set>
namespace external {namespace particles {
constexpr uint32_t Array=0xA83DC8,Vtable=0x7EF954;constexpr int Kind=7;
struct State {uint32_t object=0,id=0,type=0;int x=0,y=0,z=0,frame=0,blend=0,layer=3;};
inline int frame(int behavior,int8_t current){return behavior==0||behavior==1?int(current):-1;}
template<class Read>bool capture(Read read,uint32_t p,State&out){
 if(p<0x10000||uint64_t(p)+0x138>0x100000000ull)return false;
 uint32_t vt=0,id=0,type=0;int mode=-1,xyz[3]{};int8_t state=0;uint8_t limbo=1,onMap=0,translucency=0;int detail=0;
 if(!read(p,&vt,4)||vt!=Vtable||!read(p+0x10,&id,4)||!id||!read(p+0xAC,&type,4)||type<0x10000||uint64_t(type)+0x318>0x100000000ull||
    !read(type+0x314,&mode,4)||!read(p+0x12E,&state,1)||!read(p+0x81,&limbo,1)||limbo||!read(p+0x90,&onMap,1)||!onMap||!read(p+0x9C,xyz,sizeof xyz))return false;
 const int current=frame(mode,state);if(current<0||xyz[0]<0||xyz[1]<0||xyz[0]>=512*256||xyz[1]>=512*256||std::abs(int64_t(xyz[2]))>512*256)return false;
 State s;s.object=p;s.id=id;s.type=type;s.x=xyz[0];s.y=xyz[1];s.z=xyz[2];s.frame=current;
 if(!read(p+0x12F,&translucency,1)||!read(0xA8EB78,&detail,4))return false;
 if(detail==2)s.blend=translucency==25?1:translucency==50?2:translucency>=75?3:0;
 uint32_t again=0;if(!read(p,&again,4)||again!=vt||!read(p+0x10,&again,4)||again!=id||!read(p+0xAC,&again,4)||again!=type)return false;
 out=s;return true;
}
template<class Read>std::vector<State>all(Read read){GameView game(read);std::vector<State>out;std::set<uint32_t>seen;for(auto p:game.vec(Array,65536)){State s;if(seen.insert(p).second&&capture(read,p,s))out.push_back(s);}return out;}
template<class Assets,class Art>auto captureArtImpl(Assets&a,uint32_t p,uint32_t t,uint32_t id,Art&out,int)->decltype(a.captureParticle(p,t,id,out),bool()){return a.captureParticle(p,t,id,out);}
template<class Assets,class Art>bool captureArtImpl(Assets&,uint32_t,uint32_t,uint32_t,Art&,long){return false;}
template<class Assets,class Art>bool captureArt(Assets&a,uint32_t p,uint32_t t,uint32_t id,Art&out){return captureArtImpl(a,p,t,id,out,0);}
}}
