#pragma once
#include "passenger_bubbles.h"
#include "radar_map.h"
#include "visual_pose.h"
#include "scene_lighting.h"
#include "projection.h"
#include "building_animations.h"
#include "scene_order.h"
#include "art_variants.h"
#include "watch_focus.h"
#include "radar_shroud_selector.h"
#include <unordered_map>
namespace external { namespace watch {
constexpr int NativeViewWidth=640,NativeViewHeight=512;
struct Actor {visual::Pose pose;Target target;uint32_t type=0,owner=0,color=0;int x=0,y=0,z=0,kind=-1,health=0,stage=0,drawZ=0,sort=0,layer=2;Target parent;bool onBridge=false;uint16_t name[40]{};lighting::State light;};
struct Tile {uint32_t image=0,overlay=0,type=0,overlayType=0;int sub=0,level=0,overlayFrame=0,overlayY=0;bool bridge=false;lighting::State light,overlayLight;};
struct Frame {Target focus;Anchor center;int cx=0,cy=0,focusCx=0,focusCy=0;int radius=5,side=11;std::vector<uint32_t> cells=std::vector<uint32_t>(121);std::vector<Tile> tiles=std::vector<Tile>(121);std::vector<radar::ShroudFrames> shade=std::vector<radar::ShroudFrames>(121);std::vector<Actor> actors;std::vector<passengers::Group> passengers;bool dangerous=false,cameraReady=false;lighting::State light;double zScale=Projection{}.zScale,cameraX=0,cameraY=0;mapbounds::Bounds bounds;std::vector<repairvisuals::Sprite> repairs;};
template<class Read>void repairSprites(Read read,Frame&out){
 out.repairs.clear();for(const auto&a:out.actors)if(a.kind==3){repairvisuals::Sprite sprite;if(repairvisuals::sample(read,a.target.pointer,a.target.id,sprite))out.repairs.push_back(sprite);}
}
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
 GameView<Read> g(read);auto center=anchor(read,out.focus,out.center);if(!center.located||mapbounds::sample(read)!=out.bounds)return false;
 int fx=center.x,fy=center.y;
 if(!(center.target==out.center.target)||fx/256!=out.focusCx||fy/256!=out.focusCy)return false;
 auto light=lighting::sample(read);double zScale=projectionZScale(read);
 if(light!=out.light||zScale!=out.zScale)return false;
 for(auto&a:out.actors){uint32_t p=a.target.pointer;
  if(a.kind==projectiles::Kind){projectiles::State s;if(!projectiles::capture(read,p,s)||s.id!=a.target.id||s.type!=a.type)return false;
   a.owner=s.owner;a.x=s.x;a.y=s.y;a.z=s.z;a.pose=s.pose;a.drawZ=s.drawZ;a.sort=s.sort;a.layer=s.layer;a.onBridge=s.onBridge;a.light=projectiles::light(read,s);
   int cx=a.x/256-out.cx+out.radius,cy=a.y/256-out.cy+out.radius;if(cx<0||cy<0||cx>=out.side||cy>=out.side||!out.cells[cy*out.side+cx])return false;continue;}
  if(a.kind==5){animations::State state;if(!animations::capture(read,p,a.parent.pointer,a.parent.id,state)||state.type!=a.type||state.id!=a.target.id)return false;a.x=state.x;a.y=state.y;a.z=state.z;a.pose.frame=state.frame;a.drawZ=state.zAdjust;a.sort=state.sort;a.layer=state.layer;a.onBridge=g.template get<uint8_t>(state.parent+0x8C)!=0;a.light=lighting::animation(read,a.target.pointer,a.type,a.x,a.y,a.z,out.light);continue;}
  if(a.kind==4){if(g.template get<uint32_t>(p)!=0x7F522C||g.template get<uint32_t>(p+0x10)!=a.target.id||g.template get<uint8_t>(p+0xCD)||g.template get<int>(p+0x6C)<=0||g.template get<uint32_t>(p+0xC8)!=a.type)return false;a.stage=g.template get<int>(p+0xAC);}
  else{if(!valid(g,a.target)||g.kind(p)!=a.kind||g.type(p,a.kind)!=a.type||g.template get<uint32_t>(p+0x21C)!=a.owner)return false;a.health=g.template get<int>(p+0x6C);a.pose=visual::pose(g,p,a.type,a.kind,g.template get<int>(0xA8ED84));}
  a.layer=std::clamp(g.template get<int>(p+0x94,2),0,4);a.onBridge=g.template get<uint8_t>(p+0x8C)!=0;a.x=g.template get<int>(p+0x9C,-1);a.y=g.template get<int>(p+0xA0,-1);a.z=g.template get<int>(p+0xA4);
  int cx=a.x/256-out.cx+out.radius,cy=a.y/256-out.cy+out.radius;
  if(a.x<0||a.y<0||cx<0||cy<0||cx>=out.side||cy>=out.side||!out.cells[cy*out.side+cx])return false;
  a.light=a.kind==4?lighting::terrainObject(read,a.type,a.x,a.y,a.z,out.light):lighting::object(read,a.type,a.owner,a.kind,a.x,a.y,a.z,out.light);
 }
 out.center=center;out.cameraX=(int64_t(fx)-fy)*30./256;
 out.cameraY=(int64_t(fx)+fy)*15./256-screenHeight(center.z,out.zScale)+camera::anchorY(center.kind);
 out.bounds.clampCamera(out.cameraX,out.cameraY,NativeViewWidth,NativeViewHeight);
 out.passengers.clear();out.dangerous=false;passengers::contents(read,out.focus.pointer,out.passengers,out.dangerous);
 repairSprites(read,out);
 return life(read,out.focus).state==Life::Present;
}
template<class Read> Frame sample(Read read,Target focus,bool reveal,int radius=5,const Anchor&prior={}){
 Frame out;out.radius=std::clamp(radius,5,24);out.side=2*out.radius+1;out.cells.resize(out.side*out.side);out.tiles.resize(out.side*out.side);out.shade.resize(out.side*out.side);GameView<Read> g(read);out.center=anchor(read,focus,prior);if(!out.center.located)return out;
 out.bounds=mapbounds::sample(read);if(!out.bounds.valid())return {};
 int fx=out.center.x,fy=out.center.y;
 out.focusCx=fx/256;out.focusCy=fy/256;
 uint32_t cells=g.template get<uint32_t>(radar::Map+0x13C);int count=g.template get<int>(radar::Map+0x140);if(cells<0x10000||count<1||count>512*512)return {};
 std::array<int8_t,256> frameLookup{};
 if(!reveal&&!shroudselector::readLookup(read,frameLookup))return {};
 struct State {uint8_t status=0;shroudselector::Bits bits;};
 std::unordered_map<int,State> states;states.reserve(size_t(out.side+2)*(out.side+2));
 auto fetch=[&](int x,int y,shroudselector::Bits&bits){
  if(!out.bounds.legal(x,y)||y*512+x>=count)return false;
  auto&stored=states[y*512+x];if(!stored.status){
   stored.status=2;uint32_t cell=g.template get<uint32_t>(cells+uint32_t(y*512+x)*4);
   if(cell>=0x10000&&g.template get<int16_t>(cell+0x24)==x&&g.template get<int16_t>(cell+0x26)==y){
    std::array<uint8_t,0x15> values{};
    if(read(cell+0x12C,values.data(),values.size())){stored.bits={values[0],values[0x14]};stored.status=1;}
   }
  }
  if(stored.status!=1)return false;bits=stored.bits;return true;
 };
 auto tileTypes=g.vec(0xA8ED28,65536),overlayTypes=g.vec(0xA83D80);auto resourcePools=artvariants::pools(read,overlayTypes);
 out.focus=focus;out.light=lighting::sample(read);out.zScale=projectionZScale(read);passengers::contents(read,focus.pointer,out.passengers,out.dangerous);
 // Inventory follows the clamped camera, not the unclamped target cell. At
 // map corners those centers can differ by >20 cells on one diagonal.
 uint32_t focusCell=lighting::cellAt(read,fx,fy);int ground=focusCell?g.template get<int8_t>(focusCell+0x11B):0;
 out.cameraX=(int64_t(fx)-fy)*30./256;
 out.cameraY=(int64_t(fx)+fy)*15./256-screenHeight(out.center.z,out.zScale)+camera::anchorY(out.center.kind);
 out.bounds.clampCamera(out.cameraX,out.cameraY,NativeViewWidth,NativeViewHeight);out.cameraReady=true;
 double difference=out.cameraX/30,sum=out.cameraY/15+ground;
 out.cx=int(std::floor((sum+difference)/2));out.cy=int(std::floor((sum-difference)/2));
 for(int y=-out.radius;y<=out.radius;++y)for(int x=-out.radius;x<=out.radius;++x){int cx=out.cx+x,cy=out.cy+y;if(!out.bounds.legal(cx,cy)||cy*512+cx>=count)continue;
  uint32_t p=g.template get<uint32_t>(cells+uint32_t(cy*512+cx)*4);if(p<0x10000||g.template get<int16_t>(p+0x24)!=cx||g.template get<int16_t>(p+0x26)!=cy)continue;
  // Native tactical drawing gates cells with CoordinatesLegal, not the
  // narrower movement/usable rectangle. Its border scenery fills the view
  // above a camera-clamped target and is cropped only by the viewport.
  shroudselector::Bits verified{};
  if(!reveal&&!fetch(cx,cy,verified))return {};
  int8_t shroud=reveal?-1:shroudselector::frame(cx,cy,false,frameLookup,fetch);
  int8_t fog=reveal?-1:shroudselector::frame(cx,cy,true,frameLookup,fetch);
  if(!reveal&&(shroud<-2||shroud>46||fog<-2||fog>46))continue;
  out.shade[(y+out.radius)*out.side+x+out.radius]=reveal?radar::ShroudFrames{-1,-1}:radar::ShroudFrames{shroud,fog};
  // Native edge frames expose only part of a cell; compose their mask after
  // all terrain and objects, at the flat cell's position without Level.
  if(!reveal&&shroud==-2)continue;
  auto&t=out.tiles[(y+out.radius)*out.side+x+out.radius];t.level=g.template get<int8_t>(p+0x11B);t.sub=g.template get<uint8_t>(p+0x11A);
  t.light=lighting::cell(read,p,out.light,lighting::Layer::Terrain);t.overlayLight=t.light;
  int type=g.template get<int>(p+0x38,-1);if(type==65535){type=g.template get<int>(0xAA10B0,-1);t.sub=0;}
  if(type>=0&&size_t(type)<tileTypes.size()){t.type=artvariants::tile(read,tileTypes[type],unsigned(t.sub),cx,cy,g.template get<uint32_t>(p+0x140));t.image=g.template get<uint32_t>(t.type+0xA4); }
  int overlay=g.template get<int>(p+0x44,-1),drawOverlay=artvariants::overlay(read,overlayTypes,resourcePools,overlay,cx,cy,g.template get<uint8_t>(p+0x11C));if(drawOverlay>=0&&size_t(drawOverlay)<overlayTypes.size()){t.overlayType=overlayTypes[drawOverlay];t.overlay=g.template get<uint32_t>(t.overlayType+0xA4);t.overlayFrame=g.template get<uint8_t>(p+0x11E);}
  if(t.overlayType){t.overlayY=overlayOffset(read,p,overlayTypes[overlay],overlay,t.overlayFrame);
   if(g.template get<uint32_t>(p+0x140)&0x80){t.bridge=true;t.overlayLight=lighting::cell(read,p,out.light,lighting::Layer::Bridge);}
   else if(g.template get<uint8_t>(overlayTypes[overlay]+0x2A9)){t.overlayLight=out.light;t.overlayLight.intensity=1000;}
   else if(g.template get<uint8_t>(overlayTypes[overlay]+0x2A8))t.overlayLight=lighting::cell(read,p,out.light,lighting::Layer::Object,false);
  }
  out.cells[(y+out.radius)*out.side+x+out.radius]=0xff000000u|radar::terrainColor(g.template get<int>(p+0xEC,-1),g.template get<int8_t>(p+0x11B),!reveal&&fog!=-1);
 }
 for(auto p:g.vec(0xA8EC78)){Target t{p,g.template get<uint32_t>(p+0x10)};if(!valid(g,t))continue;
  int x=g.template get<int>(p+0x9C,-1),y=g.template get<int>(p+0xA0,-1);if(x<0||y<0)continue;int cx=x/256-out.cx,cy=y/256-out.cy;if(cx<-out.radius||cx>out.radius||cy<-out.radius||cy>out.radius||!out.cells[(cy+out.radius)*out.side+cx+out.radius])continue;
  Actor a;a.target=t;a.onBridge=g.template get<uint8_t>(p+0x8C)!=0;a.x=x;a.y=y;a.kind=g.kind(p);a.type=g.type(p,a.kind);a.owner=g.template get<uint32_t>(p+0x21C);a.health=g.template get<int>(p+0x6C);a.z=g.template get<int>(p+0xA4);a.pose=visual::pose(g,p,a.type,a.kind,g.template get<int>(0xA8ED84));uint8_t rgb[3]{180,180,180};read(a.owner+22265,rgb,3);a.color=0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];
  uint32_t name=g.template get<uint32_t>(a.type+0x60);if(!name||!read(name,a.name,sizeof a.name)){char id[24]{};read(a.type+0x24,id,23);for(int i=0;i<23;++i)a.name[i]=uint8_t(id[i]);}a.name[39]=0;
  a.layer=std::clamp(g.template get<int>(p+0x94,2),0,4);if(a.kind==3)a.sort=(g.template get<uint8_t>(a.type+0x16C5)?32:0)-(g.template get<uint8_t>(a.type+0x16B7)?16:0);
  a.light=lighting::object(read,a.type,a.owner,a.kind,a.x,a.y,a.z,out.light);out.actors.push_back(a);
 }
 // Trees and static terrain are ObjectClass instances, outside TechnoClass::Array.
 for(auto p:g.vec(0xA8E988)){if(g.template get<uint32_t>(p)!=0x7F522C||g.template get<uint8_t>(p+0xCD)||g.template get<int>(p+0x6C)<=0)continue;
  int x=g.template get<int>(p+0x9C,-1),y=g.template get<int>(p+0xA0,-1);if(x<0||y<0)continue;int cx=x/256-out.cx,cy=y/256-out.cy;if(cx<-out.radius||cx>out.radius||cy<-out.radius||cy>out.radius||!out.cells[(cy+out.radius)*out.side+cx+out.radius])continue;
  Actor a;a.target={p,g.template get<uint32_t>(p+0x10)};a.x=x;a.y=y;a.z=g.template get<int>(p+0xA4);a.type=g.template get<uint32_t>(p+0xC8);a.kind=4;a.stage=g.template get<int>(p+0xAC);a.color=0xff83a95bu;a.light=lighting::terrainObject(read,a.type,a.x,a.y,a.z,out.light);out.actors.push_back(a);
 }
 size_t bodies=out.actors.size();for(size_t i=0;i<bodies;++i){auto body=out.actors[i];if(body.kind!=3)continue;
  for(const auto&state:animations::attached(read,body.target.pointer,body.target.id)){Actor a;a.target={state.object,state.id};a.parent=body.target;a.onBridge=body.onBridge;a.type=state.type;a.owner=state.owner;a.x=state.x;a.y=state.y;a.z=state.z;a.kind=5;a.pose.frame=state.frame;a.drawZ=state.zAdjust;a.sort=state.sort;a.layer=state.layer;a.light=lighting::animation(read,a.target.pointer,a.type,a.x,a.y,a.z,out.light);out.actors.push_back(a);}
 }
 std::set<uint32_t> seen;for(const auto&a:out.actors)if(a.kind==5)seen.insert(a.target.pointer);
 for(const auto&state:animations::all(read)){if(!seen.insert(state.object).second)continue;
  int cx=state.x/256-out.cx+out.radius,cy=state.y/256-out.cy+out.radius;
  if(cx<0||cy<0||cx>=out.side||cy>=out.side||!out.cells[cy*out.side+cx])continue;
  Actor a;a.target={state.object,state.id};a.type=state.type;a.owner=state.owner;a.x=state.x;a.y=state.y;a.z=state.z;a.kind=5;a.pose.frame=state.frame;a.drawZ=state.zAdjust;a.sort=state.sort;a.layer=state.layer;a.light=lighting::animation(read,a.target.pointer,a.type,a.x,a.y,a.z,out.light);out.actors.push_back(a);
 }
 for(const auto&s:projectiles::all(read)){
  int cx=s.x/256-out.cx+out.radius,cy=s.y/256-out.cy+out.radius;if(cx<0||cy<0||cx>=out.side||cy>=out.side||!out.cells[cy*out.side+cx])continue;
  Actor a;a.target={s.object,s.id};a.type=s.type;a.owner=s.owner;a.x=s.x;a.y=s.y;a.z=s.z;a.kind=projectiles::Kind;a.pose=s.pose;a.drawZ=s.drawZ;a.sort=s.sort;a.layer=s.layer;a.onBridge=s.onBridge;a.light=projectiles::light(read,s);out.actors.push_back(a);
 }
 repairSprites(read,out);
 if(life(read,focus).state!=Life::Present||mapbounds::sample(read)!=out.bounds)return {};std::stable_sort(out.actors.begin(),out.actors.end(),[](const Actor&a,const Actor&b){return int64_t(a.x)+a.y<int64_t(b.x)+b.y;});return out;
}
}}
