#pragma once
#include "game_view.h"
#include "building_components.h"
#include "voxel_posture.h"
namespace external { namespace visual {
constexpr double Pi=3.14159265358979323846;
// FacingClass::Current (4C93D0): interpolate the signed shortest turn using
// the simulation timer, rather than drawing DesiredFacing ahead of the unit.
template<class Read>uint16_t facing(GameView<Read>&g,uint32_t p,int frame){
 uint16_t desired=g.template get<uint16_t>(p),start=g.template get<uint16_t>(p+4);
 int rate=g.template get<int16_t>(p+20),timer=g.template get<int>(p+8,-1),duration=g.template get<int>(p+16);
 int left=timer==-1?duration:std::max(0,duration-(frame-timer));
 int diff=int16_t(uint16_t(desired-start)),steps=rate>0?std::abs(diff)/rate:0;
 return rate>0&&left>0&&left<=32768&&steps>0?uint16_t(desired-(diff/steps)*left):desired;
}
inline unsigned bucket(uint16_t raw){return ((unsigned(raw)+1024)>>11)&31;}
// FA2 loads voxel direction i at Z=45*i+90 and stores it as (7-i).
// Its clockwise Z rotation followed by X=300 degrees is equivalent to our
// isometric projection with this positive world-space angle, not its inverse.
inline double radians(unsigned direction){return (int(direction)-8)*(2*Pi/32);}
inline unsigned previewBody(int heading){return unsigned(8+heading)&31;}
struct Pose {
 uint8_t body=0,turret=0;int frame=0,variant=-1,buildingFlags=0,doorFrame=-1;
 // Presentation metadata, never written to the engine. Fine angles are 1/256
 // turns; raw facing retains the already verified FacingClass::Current value.
 int bodyFine=-1,turretFine=-1,frameFrom=-1,frameWeight=255;
 uint16_t rawBody=0,rawTurret=0;bool rawFacing=false,voxel=false;
 voxelposture::State posture;
};
inline double fineRadians(int fine,unsigned fallback){return fine>=0?(double(fine)-64)*(2*Pi/256):radians(fallback);}
template<class Read>int infantryFrame(GameView<Read>&g,uint32_t type,int sequence,int stage,unsigned body){
 uint32_t list=g.template get<uint32_t>(type+0xE3C);if(sequence<0||sequence>=42||list<0x10000)return -1;
 uint32_t s=list+uint32_t(sequence)*36;int start=g.template get<int>(s,-1),count=g.template get<int>(s+4),stride=g.template get<int>(s+8);
 int dir=g.template get<int>(0x7EAEFC+(body&31)*4,-1);
 if(start<0||start>=8192||count<0||count>8192||stride<0||stride>8192||dir<0||dir>7||stage<0)return -1;
 int result=start+stage%std::max(1,count)+dir*stride;return result<8192?result:-1;
}
template<class Read>int previewFrame(GameView<Read>&g,uint32_t type,int kind,int heading){
 unsigned body=previewBody(heading);
 if(kind==1)return infantryFrame(g,type,0,0,body); // Ready, no animation advance
 if(kind==2&&g.template get<int>(type+0xE3C)==8){int dir=((body+2)/4+1)&7,stand=g.template get<int>(type+0xE1C);return stand>0?g.template get<int>(type+0xE28)+dir*stand:g.template get<int>(type+0xE2C)+dir*std::max(0,int(g.template get<int8_t>(type+0xE5C)));}
 return 0;
}
template<class Read>Pose pose(GameView<Read>&g,uint32_t p,uint32_t type,int kind,int frame){
 Pose out;out.voxel=kind!=1&&g.template get<uint32_t>(type+0xB0)!=0;out.rawBody=facing(g,p+0x388,frame);out.rawTurret=g.template get<uint8_t>(type+0xCA1)?facing(g,p+0x3A0,frame):out.rawBody;out.rawFacing=true;
 out.body=bucket(out.rawBody);out.turret=bucket(out.rawTurret);
 if(out.voxel)out.posture=voxelposture::capture(g,p,type,kind,frame);
 if(g.template get<int>(type+0x808)>0&&!g.template get<uint8_t>(type+0xCD5)){int variant=g.template get<int>(p+0x124,-1);if(variant>=0&&variant<18)out.variant=variant;}
 if(kind==1){
  int sequence=g.template get<int>(p+0x6C4);if(sequence==-1)sequence=0;
  // Native Infantry::ShapeNumber 518D80 uses this direction lookup, not the
  // SequenceFacing metadata (which is consumed when loading the sequence).
  out.frame=infantryFrame(g,type,sequence,g.template get<int>(p+0xF8),out.body);
 }else if(kind==2&&!g.template get<uint32_t>(type+0xB0)){
  int dir=g.template get<int>(type+0xE3C)==8?((unsigned(facing(g,p+0x388,frame))+4096)/8192+1)&7:0;
  int firing=g.template get<int>(p+0x6C0,-1),stand=g.template get<int>(type+0xE1C),walk=g.template get<int8_t>(type+0xE5C);
  if(firing>=0)out.frame=g.template get<int>(type+0xE30)+dir*g.template get<int8_t>(type+0xE5D)+firing/2;
  else if(stand>0)out.frame=g.template get<int>(type+0xE28)+dir*stand;
  else out.frame=g.template get<int>(type+0xE2C)+dir*std::max(0,walk)+std::max(0,g.template get<int>(p+0x538))%std::max(1,walk);
 }
 if(kind==3&&buildings::buildup(g,p))out.variant=-2;
 if(out.frame<0||out.frame>=8192)out.frame=-1;return out;
}
}}
