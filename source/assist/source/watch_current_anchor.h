#pragma once
#include "../../external_reader/source/radar_publication.h"
#include "../../external_reader/source/watch_focus.h"
#include "../../external_reader/source/native_draw_state.h"
#include "../../external_reader/source/material_motion.h"
namespace watchscene {
// Shared-frame presentation never patches a source with newer coordinates.
// The body (possibly presentation-interpolated) and its camera use the same
// marker. Identity/succession guards still run independently before this pure
// projection; a verified last point has an empty target and remains fixed.
inline bool sampledAnchors(const external::radar::Frame&frame,std::array<external::watch::Anchor,3>&anchors){
 if(!frame.terrain||!frame.markerScanValid)return false;
 for(auto&anchor:anchors){
  if(!anchor.located||!anchor.target)continue;
  const auto found=std::find_if(frame.markers.begin(),frame.markers.end(),[&](const auto&m){return m.object==anchor.target.pointer&&m.id==anchor.target.id;});
  if(found==frame.markers.end()){anchor.located=false;continue;}
  const auto&m=*found;
  if(m.kind<0||m.kind>4||m.x<0||m.y<0||m.z<0||!frame.terrain->geo.legal(m.x/256,m.y/256)){anchor.located=false;continue;}
  anchor.x=m.x;anchor.y=m.y;anchor.z=m.z;anchor.kind=m.kind;
 }
 return true;
}
// Update only this watch presentation's private Frame. The shared source bank
// keeps its original timestamp and observations. A fresh camera must not look
// at an older copy of its own target, shadow or carrier.
template<class Read>bool currentAnchors(external::radar::Frame&frame,std::array<external::watch::Anchor,3>&anchors,Read read,bool fine=false){
 using namespace external;
 const auto world=radar::publicationContext(read);if(!radar::sameWorld(frame,world))return false;
 int chrono=0;if(!read(0xA9FAB0,&chrono,4)||chrono<0||chrono>3)return false;
 auto refresh=[&](watch::Anchor&anchor){
  if(!anchor.located||!anchor.target)return true; // A hidden passenger retains its verified last center.
  auto found=std::find_if(frame.markers.begin(),frame.markers.end(),[&](const auto&m){return m.object==anchor.target.pointer&&m.id==anchor.target.id;});
  if(found==frame.markers.end())return false;
  auto&m=*found;
  if(chrono||(frame.extras&&frame.extras->chronoValid&&frame.extras->chrono.phase>0)||m.disguised){
   // Chrono owns a retained world/view pair; do not splice newer actors into it.
   anchor.x=m.x;anchor.y=m.y;anchor.z=m.z;anchor.kind=m.kind;return true;
  }
  if(m.kind<0||m.kind>3||m.kind!=anchor.kind)return false;
  const int cx=anchor.x/256,cy=anchor.y/256;const size_t index=size_t(cy)*512+cx;
  if(anchor.x<0||anchor.y<0||anchor.z<0||!frame.terrain->geo.legal(cx,cy)||index>=frame.terrain->art.size()||index>=frame.terrain->colors.size()||!frame.terrain->colors[index])return false;
  struct State {uint32_t vt=0,id=0,type=0,owner=0,cell=0;std::array<int,3>xyz{};int layer=0;uint8_t bridge=0;bool operator==(const State&o)const{return vt==o.vt&&id==o.id&&type==o.type&&owner==o.owner&&cell==o.cell&&xyz==o.xyz&&layer==o.layer&&bridge==o.bridge;}} before,after;
  const uint32_t typeAt=m.object+(m.kind==3?0x520:m.kind==1?0x6C0:0x6C4);
  auto capture=[&](State&s){return read(m.object,&s.vt,4)&&read(m.object+0x10,&s.id,4)&&read(typeAt,&s.type,4)&&read(m.object+0x21C,&s.owner,4)&&read(m.object+0x9C,s.xyz.data(),sizeof s.xyz)&&read(m.object+0x94,&s.layer,4)&&read(m.object+0x8C,&s.bridge,1)&&read(frame.terrain->geo.cells+uint32_t(index)*4,&s.cell,4);};
  if(!capture(before)||before.id!=m.id||before.type!=m.type||before.owner!=m.owner||before.layer<0||before.layer>4||before.cell!=frame.terrain->art[index].cell||before.cell<0x10000||before.xyz!=std::array<int,3>{{anchor.x,anchor.y,anchor.z}})return false;
  const uint32_t expected=m.kind==0?0x7E22A4:m.kind==1?0x7EB058:m.kind==2?0x7F5C70:0x7E3EBC;if(before.vt!=expected)return false;
  // Attached building animations have their own sampled world positions.
  // A moving/teleported building must arrive through a complete new bank.
  if(m.kind==3)return m.x==anchor.x&&m.y==anchor.y&&m.z==anchor.z&&capture(after)&&before==after;
  bool readable=true;auto checked=[&](uint32_t p,void*out,size_t n){const bool ok=read(p,out,n);readable&=ok;return ok;};
  auto draw=nativedraw::capture(checked,m.object,m.type,m.kind,frame.terrain->geo.house,world.frame);
  GameView game(checked);auto pose=external::visual::pose(game,m.object,m.type,m.kind,world.frame);
  if(!readable||!draw.visible||draw.disguised||!capture(after)||!(before==after)||!watch::life(read,anchor.target).drawable())return false;
  if(fine&&pose.voxel){pose.bodyFine=materialmotion::angle(pose.rawBody,pose.rawBody,1.);pose.turretFine=materialmotion::angle(pose.rawTurret,pose.rawTurret,1.);}
  // These are observed native poses/materials, not animation extrapolation.
  // Rebuilding the world below uses the new center for body, shadow, depth,
  // clip and lighting together, including the verified carrier of a passenger.
  m.x=anchor.x;m.y=anchor.y;m.z=anchor.z;m.pose=std::move(pose);m.material=draw.material;m.waterline=draw.waterline;m.onBridge=before.bridge!=0;m.layer=before.layer;
  frame.terrain->geo.project(double(m.x)/256,double(m.y)/256,m.u,m.v);
  return true;
 };
 // A single unavailable target keeps its old image; it cannot freeze the
 // other two independently verified cameras or clear the user's tracking.
 for(auto&anchor:anchors)if(!refresh(anchor))anchor.located=false;
 const auto after=radar::publicationContext(read);
 int chronoAfter=0;
 return radar::sameWorld(frame,after)&&after.frame==world.frame&&read(0xA9FAB0,&chronoAfter,4)&&chronoAfter==chrono;
}
}
