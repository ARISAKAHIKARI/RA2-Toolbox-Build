#pragma once
#include "passenger_bubbles.h"
#include "radar_map.h"
#include "visual_pose.h"
#include "scene_lighting.h"
#include "projection.h"
#include "building_animations.h"
namespace external { namespace watch {
struct Target {uint32_t pointer=0,id=0;bool operator==(const Target&o)const{return pointer==o.pointer&&id==o.id;}explicit operator bool()const{return pointer&&id;}};
struct Actor {visual::Pose pose;Target target;uint32_t type=0,owner=0,color=0;int x=0,y=0,z=0,kind=-1,health=0,stage=0,drawZ=0,sort=0,layer=2;Target parent;bool onBridge=false;uint16_t name[40]{};lighting::State light;};
struct Tile {uint32_t image=0,overlay=0,type=0,overlayType=0;int sub=0,level=0,overlayFrame=0,overlayY=0;bool bridge=false;lighting::State light,overlayLight;};
struct Frame {Target focus;int cx=0,cy=0;int radius=5,side=11;std::vector<uint32_t> cells=std::vector<uint32_t>(121);std::vector<Tile> tiles=std::vector<Tile>(121);std::vector<Actor> actors;std::vector<passengers::Group> passengers;bool dangerous=false;lighting::State light;double zScale=Projection{}.zScale;};
template<class Read>int overlayOffset(Read read,uint32_t cell,uint32_t type,int index,int frame){
 GameView<Read> g(read);int offset=0;
 // Native OverlayType::GetOffset (5FDCC0), then Cell::GetOverlayOffset (480110).
 if(g.template get<uint8_t>(type+0x2A9)||g.template get<uint8_t>(type+0x2A8)||g.template get<int>(type+0x294)==0x7E||g.template get<uint8_t>(type+0x2AA))offset=-12;
 if(g.template get<int>(type+0x298)==9)--offset;if(index==0x7E)--offset;
 if(g.template get<uint32_t>(cell+0x140)&0x80){offset-=16;if(frame>=9&&frame<=17)offset-=15;}else if(index==0xEF)offset-=15;
 return offset;
}
template<class Read> bool valid(GameView<Read>&g,Target t){return t&&g.live(t.pointer)&&g.template get<uint32_t>(t.pointer+0x10)==t.id&&!g.template get<uint32_t>(t.pointer+0x11C)&&g.template get<int>(t.pointer+0x220)==0;}
template<class Read> Target manual(Read read){
 GameView<Read> g(read);for(auto p:g.vec(0xA8EC78))if(g.template get<uint8_t>(p+0x431)){Target t{p,g.template get<uint32_t>(p+0x10)};if(valid(g,t))return t;}
 for(auto p:g.vec(0xA8ECB8)){Target t{p,g.template get<uint32_t>(p+0x10)};if(valid(g,t))return t;}return {};
}
template<class Read> bool danger(Read read,GameView<Read>&g,Target t,uint32_t house){
 if(!valid(g,t)||!g.enemy(t.pointer,house)||(g.kind(t.pointer)!=0&&g.kind(t.pointer)!=2))return false;
 std::vector<passengers::Group> groups;bool dangerous=false;return passengers::contents(read,t.pointer,groups,dangerous)&&dangerous;
}
template<class Read> Target choose(Read read,Target pinned,Target prior,bool automatic){
 GameView<Read> g(read);if(pinned)return valid(g,pinned)?pinned:Target{};if(!automatic)return {};
 uint32_t house=g.template get<uint32_t>(0xA83D4C);if(danger(read,g,prior,house))return prior;
 for(auto p:g.vec(0xA8EC78)){Target t{p,g.template get<uint32_t>(p+0x10)};if(danger(read,g,t,house))return t;}return {};
}
// Between simulation frames, re-read live actors at the overlay cadence while
// reusing the unchanged terrain/object inventory. No independent millisecond
// throttle: simulation changes still take the complete sampling path below.
template<class Read> bool refresh(Read read,Frame&out){
 GameView<Read> g(read);if(!valid(g,out.focus))return false;
 int fx=g.template get<int>(out.focus.pointer+0x9C,-1),fy=g.template get<int>(out.focus.pointer+0xA0,-1);
 if(fx<0||fy<0||fx/256!=out.cx||fy/256!=out.cy)return false;
 auto light=lighting::sample(read);double zScale=projectionZScale(read);
 if(light!=out.light||zScale!=out.zScale)return false;
 for(auto&a:out.actors){uint32_t p=a.target.pointer;
  if(a.kind==5){animations::State state;if(!animations::capture(read,p,a.parent.pointer,a.parent.id,state)||state.type!=a.type||state.id!=a.target.id)return false;a.x=state.x;a.y=state.y;a.z=state.z;a.pose.frame=state.frame;a.drawZ=state.zAdjust;a.sort=state.sort;a.layer=state.layer;a.onBridge=g.template get<uint8_t>(state.parent+0x8C)!=0;a.light=lighting::at(read,a.x,a.y,a.z,out.light);if(state.normalLight)a.light={};continue;}
  if(a.kind==4){if(g.template get<uint32_t>(p)!=0x7F522C||g.template get<uint32_t>(p+0x10)!=a.target.id||g.template get<uint8_t>(p+0xCD)||g.template get<int>(p+0x6C)<=0||g.template get<uint32_t>(p+0xC8)!=a.type)return false;a.stage=g.template get<int>(p+0xAC);}
  else{if(!valid(g,a.target)||g.kind(p)!=a.kind||g.type(p,a.kind)!=a.type||g.template get<uint32_t>(p+0x21C)!=a.owner)return false;a.health=g.template get<int>(p+0x6C);a.pose=visual::pose(g,p,a.type,a.kind,g.template get<int>(0xA8ED84));}
  a.layer=std::clamp(g.template get<int>(p+0x94,2),0,4);a.onBridge=g.template get<uint8_t>(p+0x8C)!=0;a.x=g.template get<int>(p+0x9C,-1);a.y=g.template get<int>(p+0xA0,-1);a.z=g.template get<int>(p+0xA4);
  int cx=a.x/256-out.cx+out.radius,cy=a.y/256-out.cy+out.radius;
  if(a.x<0||a.y<0||cx<0||cy<0||cx>=out.side||cy>=out.side||!out.cells[cy*out.side+cx])return false;
  a.light=lighting::at(read,a.x,a.y,a.z,out.light,a.kind==4);
 }
 out.passengers.clear();out.dangerous=false;passengers::contents(read,out.focus.pointer,out.passengers,out.dangerous);
 return valid(g,out.focus);
}
template<class Read> Frame sample(Read read,Target focus,bool reveal,int radius=5){
 Frame out;out.radius=std::clamp(radius,5,24);out.side=2*out.radius+1;out.cells.resize(out.side*out.side);out.tiles.resize(out.side*out.side);GameView<Read> g(read);if(!valid(g,focus))return out;
 out.cx=g.template get<int>(focus.pointer+0x9C,-256)/256;out.cy=g.template get<int>(focus.pointer+0xA0,-256)/256;if(out.cx<0||out.cy<0||out.cx>=512||out.cy>=512)return {};
 uint32_t cells=g.template get<uint32_t>(radar::Map+0x13C);int count=g.template get<int>(radar::Map+0x140);if(cells<0x10000||count<1||count>512*512)return {};
 auto tileTypes=g.vec(0xA8ED28,65536),overlayTypes=g.vec(0xA83D80);
 out.focus=focus;out.light=lighting::sample(read);out.zScale=projectionZScale(read);passengers::contents(read,focus.pointer,out.passengers,out.dangerous);
 for(int y=-out.radius;y<=out.radius;++y)for(int x=-out.radius;x<=out.radius;++x){int cx=out.cx+x,cy=out.cy+y;if(cx<0||cy<0||cx>=512||cy>=512||cy*512+cx>=count)continue;
  uint32_t p=g.template get<uint32_t>(cells+uint32_t(cy*512+cx)*4);if(p<0x10000||g.template get<int16_t>(p+0x24)!=cx||g.template get<int16_t>(p+0x26)!=cy)continue;
  bool seen=g.template get<int8_t>(p+0x120)==-1,fog=g.template get<int8_t>(p+0x121)!=-1;if(!reveal&&!seen)continue;
  auto&t=out.tiles[(y+out.radius)*out.side+x+out.radius];t.level=g.template get<int8_t>(p+0x11B);t.sub=g.template get<uint8_t>(p+0x11A);
  t.light=lighting::cell(read,p,out.light,lighting::Layer::Terrain);t.overlayLight=t.light;
  int type=g.template get<int>(p+0x38,-1);if(type==65535){type=g.template get<int>(0xAA10B0,-1);t.sub=0;}
  if(type>=0&&size_t(type)<tileTypes.size()){t.type=tileTypes[type];t.image=g.template get<uint32_t>(t.type+0xA4); }
  int overlay=g.template get<int>(p+0x44,-1);if(overlay>=0&&size_t(overlay)<overlayTypes.size()){t.overlayType=overlayTypes[overlay];t.overlay=g.template get<uint32_t>(t.overlayType+0xA4);t.overlayFrame=g.template get<uint8_t>(p+0x11E);}
  if(t.overlayType){t.overlayY=overlayOffset(read,p,t.overlayType,overlay,t.overlayFrame);
   if(g.template get<uint32_t>(p+0x140)&0x80){t.bridge=true;t.overlayLight=lighting::cell(read,p,out.light,lighting::Layer::Bridge);}
   else if(g.template get<uint8_t>(t.overlayType+0x2A9)){t.overlayLight=out.light;t.overlayLight.intensity=1000;}
   else if(g.template get<uint8_t>(t.overlayType+0x2A8))t.overlayLight=lighting::cell(read,p,out.light,lighting::Layer::Object,false);
  }
  out.cells[(y+out.radius)*out.side+x+out.radius]=0xff000000u|radar::terrainColor(g.template get<int>(p+0xEC,-1),g.template get<uint8_t>(p+0x11B),!reveal&&fog);
 }
 for(auto p:g.vec(0xA8EC78)){Target t{p,g.template get<uint32_t>(p+0x10)};if(!valid(g,t))continue;
  int x=g.template get<int>(p+0x9C,-1),y=g.template get<int>(p+0xA0,-1);if(x<0||y<0)continue;int cx=x/256-out.cx,cy=y/256-out.cy;if(cx<-out.radius||cx>out.radius||cy<-out.radius||cy>out.radius||!out.cells[(cy+out.radius)*out.side+cx+out.radius])continue;
  Actor a;a.target=t;a.onBridge=g.template get<uint8_t>(p+0x8C)!=0;a.x=x;a.y=y;a.kind=g.kind(p);a.type=g.type(p,a.kind);a.owner=g.template get<uint32_t>(p+0x21C);a.health=g.template get<int>(p+0x6C);a.z=g.template get<int>(p+0xA4);a.pose=visual::pose(g,p,a.type,a.kind,g.template get<int>(0xA8ED84));uint8_t rgb[3]{180,180,180};read(a.owner+22265,rgb,3);a.color=0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];
  uint32_t name=g.template get<uint32_t>(a.type+0x60);if(!name||!read(name,a.name,sizeof a.name)){char id[24]{};read(a.type+0x24,id,23);for(int i=0;i<23;++i)a.name[i]=uint8_t(id[i]);}a.name[39]=0;
  a.layer=std::clamp(g.template get<int>(p+0x94,2),0,4);if(a.kind==3)a.sort=(g.template get<uint8_t>(a.type+0x16C5)?32:0)-(g.template get<uint8_t>(a.type+0x16B7)?16:0);
  a.light=lighting::at(read,a.x,a.y,a.z,out.light);out.actors.push_back(a);
 }
 // Trees and static terrain are ObjectClass instances, outside TechnoClass::Array.
 for(auto p:g.vec(0xA8E988)){if(g.template get<uint32_t>(p)!=0x7F522C||g.template get<uint8_t>(p+0xCD)||g.template get<int>(p+0x6C)<=0)continue;
  int x=g.template get<int>(p+0x9C,-1),y=g.template get<int>(p+0xA0,-1);if(x<0||y<0)continue;int cx=x/256-out.cx,cy=y/256-out.cy;if(cx<-out.radius||cx>out.radius||cy<-out.radius||cy>out.radius||!out.cells[(cy+out.radius)*out.side+cx+out.radius])continue;
  Actor a;a.target={p,g.template get<uint32_t>(p+0x10)};a.x=x;a.y=y;a.z=g.template get<int>(p+0xA4);a.type=g.template get<uint32_t>(p+0xC8);a.kind=4;a.stage=g.template get<int>(p+0xAC);a.color=0xff83a95bu;a.light=lighting::at(read,a.x,a.y,a.z,out.light,true);out.actors.push_back(a);
 }
 size_t bodies=out.actors.size();for(size_t i=0;i<bodies;++i){auto body=out.actors[i];if(body.kind!=3)continue;
  for(const auto&state:animations::attached(read,body.target.pointer,body.target.id)){Actor a;a.target={state.object,state.id};a.parent=body.target;a.onBridge=body.onBridge;a.type=state.type;a.owner=state.owner;a.x=state.x;a.y=state.y;a.z=state.z;a.kind=5;a.pose.frame=state.frame;a.drawZ=state.zAdjust;a.sort=state.sort;a.layer=state.layer;a.light=lighting::at(read,a.x,a.y,a.z,out.light);if(state.normalLight)a.light={};out.actors.push_back(a);}
 }
 if(!valid(g,focus))return {};std::stable_sort(out.actors.begin(),out.actors.end(),[](const Actor&a,const Actor&b){return int64_t(a.x)+a.y<int64_t(b.x)+b.y;});return out;
}
}}
