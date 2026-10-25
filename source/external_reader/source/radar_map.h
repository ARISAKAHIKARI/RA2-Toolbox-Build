#pragma once
#include "game_view.h"
#include "display_state.h"
#include "visual_pose.h"
#include "building_animations.h"
#include "map_bounds.h"
#include "art_variants.h"
#include "repair_visuals.h"
#include "projectile_visuals.h"
#include <memory>
#include <array>
// Read-only YRpp MapClass/CellClass/RadarClass model. Projection matches
// GetCrdOnRadar (0x6557F0); never calls game functions or captures the screen.
namespace external { namespace radar {
constexpr uint32_t Map=0x87F7E8;
struct Geometry {
 int width=0,height=0,offsetX=0,offsetY=0,rw=0,rh=0;float factor=0;uint32_t cells=0,house=0;int count=0;mapbounds::Bounds bounds;
 bool valid()const{return bounds.valid()&&bounds.width==width&&bounds.height==height&&cells>=0x10000&&count>0&&count<=512*512&&rw>0&&rw<=4096&&rh>0&&rh<=4096&&std::isfinite(factor)&&factor>0.01f&&factor<32&&std::abs(int64_t(offsetX))<=1024&&std::abs(int64_t(offsetY))<=1024&&house>=0x10000;}
 bool same(const Geometry&o)const{return width==o.width&&height==o.height&&offsetX==o.offsetX&&offsetY==o.offsetY&&rw==o.rw&&rh==o.rh&&factor==o.factor&&cells==o.cells&&house==o.house&&count==o.count&&bounds==o.bounds;}
 bool legal(int x,int y)const{return bounds.legal(x,y);}
 mapbounds::NativeViewport nativeViewport()const{return valid()?mapbounds::nativeViewport(rw,rh,factor):mapbounds::NativeViewport{};}
 bool tileOrigin(int x,int y,int level,mapbounds::TileOrigin&out)const{if(!bounds.tileOrigin(x,y,level,out))return false;out.x+=offsetX*30;out.y-=offsetY*15;return true;}
 void project(double x,double y,double&u,double&v)const{u=(x-y+offsetX)*factor/rw;v=(x+y-offsetY)*factor/rh;}
};
struct Marker {double u=0,v=0;uint32_t color=0;bool building=false;uint32_t object=0,id=0,type=0,owner=0,parent=0,parentId=0;int x=0,y=0,z=0,kind=-1,drawZ=0,sort=0,layer=2;bool normalLight=false,onBridge=false;visual::Pose pose;};
struct CellArt {uint32_t cell=0,type=0,overlayType=0,overlaySourceType=0;int16_t overlay=-1;uint8_t sub=0;int8_t level=0;uint8_t overlayFrame=0;};
struct Scene {int width=0,height=0;std::vector<uint32_t> pixels;};
struct Terrain {Geometry geo;std::vector<uint32_t> colors;std::vector<CellArt> art;bool reveal=false;};
struct Frame {std::shared_ptr<const Terrain> terrain;std::vector<Marker> markers;std::shared_ptr<const Scene> scene,map;uint32_t tick=0;int frame=-1;bool markerScanValid=false;std::vector<repairvisuals::Sprite> repairs;};
inline uint32_t terrainColor(int land,int level,bool fog){
 static constexpr uint32_t colors[]={0x66794b,0x85836c,0x285e86,0x70756d,0x999684,0xb7a843,0xb4a47c,0x77734d,0xb4cbd0,0x77766b,0x55584f,0x58703d};
 if(land<0||land>=12)return 0x44483a;uint32_t c=colors[land];int shade=std::clamp(level,0,14)*2;
 int r=std::min(255,int(c>>16)+shade),g=std::min(255,int((c>>8)&255)+shade),b=std::min(255,int(c&255)+shade);if(fog){r=r*2/3;g=g*2/3;b=b*2/3;}return uint32_t(r<<16|g<<8|b);
}
template<class Read> class Sampler {
 Read read_;GameView<Read> view_;Geometry geo_{};std::vector<uint32_t> pointers_;std::shared_ptr<Terrain> pending_;std::shared_ptr<const Terrain> terrain_;
 std::vector<uint32_t> tileTypes_,overlayTypes_;std::vector<artvariants::Pool> resourcePools_;size_t cursor_=0;int lastFrame_=-1;uint32_t reveal_=0;unsigned revision_=0;
 template<class T>T get(uint32_t a,T fallback={})const{return view_.template get<T>(a,fallback);}
public:
 explicit Sampler(Read r):read_(r),view_(r){}
 void reset(){geo_={};pointers_.clear();pending_.reset();terrain_.reset();cursor_=0;lastFrame_=-1;++revision_;}
 unsigned revision()const{return revision_;}
 bool step(uint32_t reveal,int budget=4096,bool real=false){
  // These are preview pixels, not the game's half-opacity shroud blitter.
  // Every display override exposes the unmasked minimap; native mode alone
  // follows the explored/fog cells. Keep this policy in the immutable bank.
  reveal=reveal?1u:0u;
  Geometry g;g.bounds=mapbounds::sample(read_);g.width=g.bounds.width;g.height=g.bounds.height;g.cells=get<uint32_t>(Map+0x13C);g.count=get<int>(Map+0x140);
  g.factor=get<float>(Map+0x1488);g.offsetX=get<int>(Map+0x1490);g.offsetY=get<int>(Map+0x1498);g.rw=get<int>(Map+0x14A4);g.rh=get<int>(Map+0x14A8);g.house=get<uint32_t>(0xA83D4C);
  int frame=get<int>(0xA8ED84,-1);if(!g.valid()||frame<0||get<uint8_t>(0xA8E9A0)!=1 ){reset();return false;}
  if(!display::inspect(read_).visible())return false;
  if(!g.same(geo_)||frame<lastFrame_||reveal_!=reveal){reset();geo_=g;reveal_=reveal;}lastFrame_=frame;
  if(!pending_){pointers_.resize(g.count);if(!read_(g.cells,pointers_.data(),pointers_.size()*4)){reset();return false;}
   pending_=std::make_shared<Terrain>();pending_->geo=g;pending_->reveal=bool(reveal);pending_->colors.resize(512*512);if(real){pending_->art.resize(512*512);tileTypes_=view_.vec(0xA8ED28,65536);overlayTypes_=view_.vec(0xA83D80);resourcePools_=artvariants::pools(read_,overlayTypes_);}cursor_=0;}
  size_t end=std::min(pointers_.size(),cursor_+size_t(std::clamp(budget,1,16384)));
  // Retain drawable physical cells around the usable rectangle. Native radar
  // projection/canvas cropping and live camera limits define the visible view.
  for(;cursor_<end;++cursor_){int x=int(cursor_%512),y=int(cursor_/512);if(!g.legal(x,y))continue;uint32_t p=pointers_[cursor_];
   if(p<0x10000)continue;std::array<uint8_t,0x148> cell{};if(!read_(p,cell.data(),cell.size()))continue;
   int16_t cx=0,cy=0;int land=-1;std::memcpy(&cx,cell.data()+0x24,2);std::memcpy(&cy,cell.data()+0x26,2);std::memcpy(&land,cell.data()+0xEC,4);
   if(cx!=x||cy!=y)continue;int visibility=int8_t(cell[0x120]),fog=int8_t(cell[0x121]);if(!reveal&&visibility!=-1)continue;
   uint32_t color=terrainColor(land,int8_t(cell[0x11B]),!reveal&&fog!=-1);
   pending_->colors[cursor_]=color;
   if(real&&!pending_->art.empty()){auto&a=pending_->art[cursor_];a.cell=p;a.sub=cell[0x11A];a.level=int8_t(cell[0x11B]);a.overlayFrame=cell[0x11E];int tile=-1,overlay=-1;uint32_t flags=0;memcpy(&flags,cell.data()+0x140,4);memcpy(&tile,cell.data()+0x38,4);memcpy(&overlay,cell.data()+0x44,4);if(tile==65535){tile=get<int>(0xAA10B0,-1);a.sub=0;}if(tile>=0&&size_t(tile)<tileTypes_.size())a.type=artvariants::tile(read_,tileTypes_[tile],a.sub,x,y,flags);if(overlay>=0&&size_t(overlay)<overlayTypes_.size()){a.overlay=int16_t(overlay);a.overlaySourceType=overlayTypes_[overlay];int drawOverlay=artvariants::overlay(read_,overlayTypes_,resourcePools_,overlay,x,y,cell[0x11C]);a.overlayType=overlayTypes_[drawOverlay];}}
  }
  if(cursor_==pointers_.size()){terrain_=pending_;pending_.reset();++revision_;}return true;
 }
 Frame snapshot(uint32_t now)const{
  GameView<Read> live(read_);Frame out;out.terrain=terrain_;out.tick=now;out.frame=lastFrame_;if(!terrain_)return out;
  std::vector<uint32_t> bodies;out.markerScanValid=view_.vecChecked(0xA8EC78,bodies);
  if(!out.markerScanValid)return out;
  for(uint32_t p:bodies){int kind=view_.kind(p);if(!view_.live(p)||(kind!=3&&get<uint32_t>(p+0x11C))||get<int>(p+0x220)!=0)continue;
   int x=get<int>(p+0x9C,-1),y=get<int>(p+0xA0,-1);if(x<0||y<0||x>=512*256||y>=512*256)continue;int cx=x/256,cy=y/256;
   if(!geo_.legal(cx,cy)||!terrain_->colors[cy*512+cx])continue;uint32_t owner=get<uint32_t>(p+0x21C);uint8_t rgb[3]{};if(owner<0x10000||!read_(owner+22265,rgb,3))continue;
   Marker m;geo_.project(double(x)/256,double(y)/256,m.u,m.v);if(m.u<0||m.u>=1||m.v<0||m.v>=1)continue;
   m.color=uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2];m.building=kind==3;m.object=p;m.id=get<uint32_t>(p+0x10);m.type=view_.type(p,kind);m.owner=owner;m.x=x;m.y=y;m.z=get<int>(p+0xA4);m.kind=kind;m.onBridge=get<uint8_t>(p+0x8C)!=0;m.pose=visual::pose(live,p,m.type,kind,lastFrame_);
   m.layer=std::clamp(get<int>(p+0x94,2),0,4);if(kind==3)m.sort=(get<uint8_t>(m.type+0x16C5)?32:0)-(get<uint8_t>(m.type+0x16B7)?16:0);out.markers.push_back(m);
  }
  if(!terrain_->art.empty()){
   const auto bodies=out.markers;for(const auto&body:bodies)if(body.kind==3)for(const auto&a:animations::attached(read_,body.object,body.id)){
    Marker m;m.object=a.object;m.id=a.id;m.type=a.type;m.owner=a.owner;m.parent=a.parent;m.parentId=a.parentId;m.x=a.x;m.y=a.y;m.z=a.z;m.kind=5;m.pose.frame=a.frame;m.drawZ=a.zAdjust;m.sort=a.sort;m.layer=a.layer;m.normalLight=a.normalLight;m.onBridge=get<uint8_t>(a.parent+0x8C)!=0;geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
   std::set<uint32_t> seen;for(const auto&m:out.markers)if(m.kind==5)seen.insert(m.object);
   for(const auto&a:animations::all(read_)){if(!seen.insert(a.object).second||!terrain_->colors[(a.y/256)*512+a.x/256])continue;
    Marker m;m.object=a.object;m.id=a.id;m.type=a.type;m.owner=a.owner;m.x=a.x;m.y=a.y;m.z=a.z;m.kind=5;m.pose.frame=a.frame;m.drawZ=a.zAdjust;m.sort=a.sort;m.layer=a.layer;m.normalLight=a.normalLight;geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
   for(auto p:view_.vec(0xA8E988)){if(get<uint32_t>(p)!=0x7F522C||get<uint8_t>(p+0xCD)||get<int>(p+0x6C)<=0)continue;
    Marker m;m.object=p;m.id=get<uint32_t>(p+0x10);m.type=get<uint32_t>(p+0xC8);m.x=get<int>(p+0x9C,-1);m.y=get<int>(p+0xA0,-1);m.z=get<int>(p+0xA4);m.kind=4;m.pose.frame=get<int>(p+0xAC);
    if(m.x<0||m.y<0||m.x>=512*256||m.y>=512*256||!terrain_->colors[(m.y/256)*512+m.x/256])continue;
    geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
  }
  if(!terrain_->art.empty()){
   for(const auto&m:out.markers)if(m.kind==3){repairvisuals::Sprite sprite;if(repairvisuals::sample(read_,m.object,m.id,sprite))out.repairs.push_back(sprite);}
   for(const auto&s:projectiles::all(read_)){
    int cx=s.x/256,cy=s.y/256;if(!geo_.legal(cx,cy)||!terrain_->colors[cy*512+cx])continue;
    Marker m;m.object=s.object;m.id=s.id;m.type=s.type;m.owner=s.owner;m.x=s.x;m.y=s.y;m.z=s.z;m.kind=projectiles::Kind;m.pose=s.pose;m.drawZ=s.drawZ;m.sort=s.sort;m.layer=s.layer;m.onBridge=s.onBridge;
    geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
  }
  return out;
 }
};
inline uint32_t smoothColor(const Terrain&t,double x,double y){
 const auto&g=t.geo;int cx=int(std::floor(x)),cy=int(std::floor(y));
 if(!g.legal(cx,cy))return 0x101710;
 uint32_t center=t.colors[cy*512+cx];if(!center)return 0x080c08;
 x-=.5;y-=.5;int ix=int(std::floor(x)),iy=int(std::floor(y));double fx=x-ix,fy=y-iy;
 double red=0,green=0,blue=0;
 for(int j=0;j<2;++j)for(int i=0;i<2;++i){double weight=(i?fx:1-fx)*(j?fy:1-fy);int xx=ix+i,yy=iy+j;
  uint32_t c=g.legal(xx,yy)?t.colors[yy*512+xx]:0;if(!c)c=center;
  red+=((c>>16)&255)*weight;green+=((c>>8)&255)*weight;blue+=(c&255)*weight;
 }
 return (uint32_t(red+.5)<<16)|(uint32_t(green+.5)<<8)|uint32_t(blue+.5);
}
inline void raster(const Terrain&t,int w,int h,std::vector<uint32_t>&pixels){
 if(w<=0||h<=0||w>4096||h>4096){pixels.clear();return;}pixels.assign(size_t(w)*h,0x101710);if(!t.geo.valid()||t.colors.size()!=512*512)return;const auto&g=t.geo;
 for(int py=0;py<h;++py)for(int px=0;px<w;++px){double u=(px+.5)*g.rw/(w*g.factor)-g.offsetX,v=(py+.5)*g.rh/(h*g.factor)+g.offsetY;
  pixels[size_t(py)*w+px]=smoothColor(t,(u+v)/2,(v-u)/2);
 }
}
// Immutable terrain banks change independently of marker/display frames.
// Retain their scaled background so higher refresh rates do not rescale it.
class DiagramRenderer {
 std::shared_ptr<const Terrain> terrain_;std::shared_ptr<const Scene> scene_;
public:
 void reset(){terrain_.reset();scene_.reset();}
 std::shared_ptr<const Scene> render(const std::shared_ptr<const Terrain>&terrain,int width,int height){
  if(!terrain||width<1||height<1||width>2048||height>2048)return {};
  if(terrain_==terrain&&scene_&&scene_->width==width&&scene_->height==height)return scene_;
  auto scene=std::make_shared<Scene>();scene->width=width;scene->height=height;raster(*terrain,width,height,scene->pixels);
  terrain_=terrain;scene_=std::move(scene);return scene_;
 }
};
}}

