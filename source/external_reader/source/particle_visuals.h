#pragma once
#include "game_view.h"
#include <set>
namespace external {namespace particles {
constexpr uint32_t Array=0xA83DC8,Vtable=0x7EF954;constexpr int Kind=7;
struct State {uint32_t object=0,id=0,type=0;int x=0,y=0,z=0,frame=0,blend=0,layer=3,behavior=0;uint32_t color=0;bool point=false;};
inline int frame(int behavior,int8_t current){return behavior==0||behavior==1?int(current):-1;}
template<class Read>bool capture(Read read,uint32_t p,State&out){
 if(p<0x10000||uint64_t(p)+0x138>0x100000000ull)return false;
 uint32_t vt=0,id=0,type=0;int mode=-1,xyz[3]{};int8_t state=0;uint8_t limbo=1,onMap=0,translucency=0;int detail=0;
 if(!read(p,&vt,4)||vt!=Vtable||!read(p+0x10,&id,4)||!id||!read(p+0xAC,&type,4)||type<0x10000||uint64_t(type)+0x318>0x100000000ull||
    !read(type+0x314,&mode,4)||!read(p+0x12E,&state,1)||!read(p+0x81,&limbo,1)||limbo||!read(p+0x90,&onMap,1)||!onMap||!read(p+0x9C,xyz,sizeof xyz))return false;
 int current=frame(mode,state);if(mode==2){uint32_t system=0;uint8_t direction=0,end=0;if(!read(p+0x124,&system,4)||system<0x10000||!read(system+0xF4,&direction,1)||!read(type+0x309,&end,1)||!end)return false;current=((direction>>6)&3)*int(end)+uint8_t(state);}if(mode==3||mode==4)current=0;if(current<0||current>=8192||mode<0||mode>4||xyz[0]<0||xyz[1]<0||xyz[0]>=512*256||xyz[1]>=512*256||std::abs(int64_t(xyz[2]))>512*256)return false;
 State s;s.object=p;s.id=id;s.type=type;s.x=xyz[0];s.y=xyz[1];s.z=xyz[2];s.frame=current;s.behavior=mode;s.point=mode>=3;if(s.point){uint8_t rgb[3]{};if(!read(p+0xB0,rgb,3))return false;s.color=uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2];GameView game(read);uint32_t items=game.template get<uint32_t>(type+0x2BC);int count=game.template get<int>(type+0x2C8),index=game.template get<int>(p+0xD8);double phase=0;if(count>=2&&count<=4096&&index>=0&&index<count-1&&items>=0x10000&&read(p+0xD0,&phase,8)&&std::isfinite(phase)&&phase>=0&&phase<=1){uint32_t first=game.template get<uint32_t>(items+index*4),next=game.template get<uint32_t>(items+(index+1)*4);uint8_t a[3]{},b[3]{};if((index==0||read(first,a,3))&&read(next,b,3)){if(index==0)std::memcpy(a,rgb,3);s.color=0;for(int i=0;i<3;++i)s.color|=uint32_t(std::clamp(int(a[i]*(1-phase)+b[i]*phase),0,255))<<(16-8*i);}}}
 if(!read(p+0x12F,&translucency,1)||!read(0xA8EB78,&detail,4))return false;
 if(detail==2)s.blend=translucency==25?1:translucency==50?2:int8_t(translucency)>=75?3:0;
 uint32_t again=0;if(!read(p,&again,4)||again!=vt||!read(p+0x10,&again,4)||again!=id||!read(p+0xAC,&again,4)||again!=type)return false;
 out=s;return true;
}
template<class Read>std::vector<State>all(Read read){GameView game(read);std::vector<State>out;std::set<uint32_t>seen;for(auto p:game.vec(Array,65536)){State s;if(seen.insert(p).second&&capture(read,p,s))out.push_back(s);}return out;}
template<class Assets,class Art>auto captureArtImpl(Assets&a,uint32_t p,uint32_t t,uint32_t id,Art&out,int)->decltype(a.captureParticle(p,t,id,out),bool()){return a.captureParticle(p,t,id,out);}
template<class Assets,class Art>bool captureArtImpl(Assets&,uint32_t,uint32_t,uint32_t,Art&,long){return false;}
template<class Assets,class Art>bool captureArt(Assets&a,uint32_t p,uint32_t t,uint32_t id,Art&out){return captureArtImpl(a,p,t,id,out,0);}
}}
