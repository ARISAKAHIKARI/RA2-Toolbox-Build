#pragma once
#include "game_view.h"
#include "visual_pose.h"
#include "scene_lighting.h"
#include "voxel_lighting.h"
#include <cmath>
#include <set>
#include <utility>
namespace external { namespace projectiles {
constexpr uint32_t Array=0xA8ED40,Vtable=0x7E46E4,TypeVtable=0x7E4948;
constexpr int Kind=6;
enum Palette {ProjectilePalette,AnimationPalette,SchemePalette};
struct State {
 uint32_t object=0,id=0,type=0,firer=0,owner=0,converter=0;
 int x=0,y=0,z=0,layer=2,drawZ=0,sort=0,colorIndex=-1;
 bool voxel=false,flat=false,shadow=false,onBridge=false;Palette palette=ProjectilePalette;
 visual::Pose pose;
};
inline bool pointer(uint32_t p,size_t n){return p>=0x10000&&uint64_t(p)+n<=0x100000000ull;}
// 468000/468137/4681C0: native atan2 -> truncated Dir16 -> nearest Dir32.
// The 7F4890 SHP lookup is (28-direction)&31; it is not a unit facing LUT.
inline unsigned direction(double y,double x){
 constexpr double HalfPi=1.57079632679489661923,Scale=-10430.060040584269;
 int32_t fixed=int32_t((std::atan2(y,x)-HalfPi)*Scale);
 return ((((unsigned(uint16_t(fixed))>>10)+1)>>1)&31);
}
template<class Read>bool capture(Read read,uint32_t p,State&out){
 if(!pointer(p,0x15C))return false;
 uint32_t vt=0,id=0,type=0,typeVt=0;uint8_t limbo=1,onMap=0,inviso=1,nextAnim=1,voxel=0,noRotate=1,animLow=0,animHigh=0,frame=0,flat=0,shadow=0,bridge=0;
 int location[3]{};double velocity[3]{};
 if(!read(p,&vt,4)||vt!=Vtable||!read(p+0x10,&id,4)||!id||!read(p+0xAC,&type,4)||!pointer(type,0x2F8)||
    !read(type,&typeVt,4)||typeVt!=TypeVtable||!read(p+0x81,&limbo,1)||limbo||!read(p+0x90,&onMap,1)||!onMap||
    !read(type+0x29E,&inviso,1)||inviso||!read(p+0x158,&nextAnim,1)||nextAnim||
    !read(p+0x9C,location,sizeof location)||
    !read(type+0x236,&voxel,1)||!read(type+0x2A1,&noRotate,1)||!read(type+0x2F4,&animLow,1)||!read(type+0x2F5,&animHigh,1)||
    !read(p+0x12C,&frame,1)||!read(type+0x2F7,&flat,1)||!read(type+0x29A,&shadow,1)||!read(p+0x8C,&bridge,1))return false;
 if(location[0]<0||location[1]<0||location[0]>=512*256||location[1]>=512*256||std::abs(int64_t(location[2]))>512*256)return false;
 if((voxel||!noRotate)&&(!read(p+0xE8,velocity,sizeof velocity)||std::any_of(velocity,velocity+3,[](double n){return !std::isfinite(n)||std::abs(n)>1e6;})))return false;
 State s;s.object=p;s.id=id;s.type=type;s.x=location[0];s.y=location[1];s.z=location[2];s.layer=flat?1:3;s.voxel=voxel!=0;s.flat=flat!=0;s.shadow=shadow!=0;s.onBridge=bridge!=0;s.drawZ=s.voxel?0:-30;
 read(p+0xB0,&s.firer,4);if(pointer(s.firer,0x220))read(s.firer+0x21C,&s.owner,4);
 s.pose.voxel=s.voxel;
 if(s.voxel){s.pose.body=uint8_t(direction(-velocity[1],velocity[0]));
  s.pose.turret=uint8_t(direction(velocity[2],voxel::nativeVoxelSqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1])));s.pose.frame=0;
 }else{s.pose.frame=noRotate?0:int((28-direction(-velocity[1],velocity[0]))&31);if(animLow||animHigh)s.pose.frame=frame;}
 uint8_t animPalette=0,firersPalette=0;
 if(!read(type+0x2A8,&animPalette,1)||!read(type+0x2A9,&firersPalette,1))return false;
 if(s.voxel){if(!read(type+0x2D4,&s.colorIndex,4)||s.colorIndex<0||s.colorIndex>=512)return false;s.palette=SchemePalette;}
 else if(animPalette)s.palette=AnimationPalette;
 else if(firersPalette){s.palette=SchemePalette;if(!read(p+0x114,&s.colorIndex,4))return false;
  if(s.colorIndex==-1){uint32_t current=0;if(!read(0xA83D4C,&current,4)||!pointer(current,0x16058)||!read(current+0x16054,&s.colorIndex,4))return false;}
  if(s.colorIndex<0||s.colorIndex>=512)return false;
 }
 if(s.palette==SchemePalette){s.converter=lighting::schemeConverter(read,0xB054D0,s.colorIndex);if(!s.converter)return false;}
 else read(s.palette==AnimationPalette?0x87F6C0:0x87F6C4,&s.converter,4);
 // All existing source/pose cache keys include variant. A different inherited
 // remap must not reuse another bullet's already-decoded palette.
 s.pose.variant=int32_t(s.converter);uint32_t again=0;uint8_t flag=0;int after[3]{};
 if(!read(p,&again,4)||again!=vt||!read(p+0x10,&again,4)||again!=id||!read(p+0xAC,&again,4)||again!=type||
    !read(p+0x81,&flag,1)||flag!=limbo||!read(p+0x90,&flag,1)||flag!=onMap||!read(p+0x158,&flag,1)||flag!=nextAnim||
    !read(type+0x29E,&flag,1)||flag!=inviso||!read(p+0x9C,after,sizeof after)||memcmp(location,after,sizeof location))return false;
 out=s;return true;
}
// Bullet+6C is weapon damage, including zero/negative values. Never reuse
// TechnoClass health/alive predicates for this separate object family.
template<class Read>std::vector<State> all(Read read){
 GameView<Read> game(read);std::vector<State> out;std::set<uint32_t> seen;
 for(auto p:game.vec(Array,16384)){State s;if(seen.insert(p).second&&capture(read,p,s))out.push_back(s);}return out;
}
template<class Read>lighting::State light(Read read,uint32_t converter){
 lighting::State out;out.intensity=1000;out.tint={{1000,1000,1000}};
 uint32_t vt=0;if(pointer(converter,0x1B4)&&read(converter,&vt,4)&&vt==0x7ED0A4)lighting::converterTint(read,converter,out.tint);
 return out;
}
template<class Read>lighting::State light(Read read,const State&s){return light(read,s.converter);}
// Production assets implement captureProjectile. Existing renderer fixtures
// can omit it; unsupported assets produce no fabricated fallback projectile.
template<class Assets,class Art>auto captureArtImpl(Assets&a,uint32_t p,uint32_t t,uint32_t id,Art&out,int)
 ->decltype(a.captureProjectile(p,t,id,out),bool()){return a.captureProjectile(p,t,id,out);}
template<class Assets,class Art>bool captureArtImpl(Assets&,uint32_t,uint32_t,uint32_t,Art&,long){return false;}
template<class Assets,class Art>bool captureArt(Assets&a,uint32_t p,uint32_t t,uint32_t id,Art&out){return captureArtImpl(a,p,t,id,out,0);}
}}
