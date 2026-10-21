#pragma once
#include "game_view.h"
#include "display_state.h"
#include <memory>
#include <array>
// Read-only YRpp MapClass/CellClass/RadarClass model. Projection matches
// GetCrdOnRadar (0x6557F0); never calls game functions or captures the screen.
namespace external { namespace radar {
constexpr uint32_t Map=0x87F7E8;
struct Geometry {
 int width=0,height=0,offsetX=0,offsetY=0,rw=0,rh=0;float factor=0;uint32_t cells=0,house=0;int count=0;
 bool valid()const{return width>0&&width<=256&&height>0&&height<=256&&cells>=0x10000&&count>0&&count<=512*512&&rw>0&&rw<=4096&&rh>0&&rh<=4096&&std::isfinite(factor)&&factor>0.01f&&factor<32&&std::abs(int64_t(offsetX))<=1024&&std::abs(int64_t(offsetY))<=1024&&house>=0x10000;}
 bool same(const Geometry&o)const{return width==o.width&&height==o.height&&offsetX==o.offsetX&&offsetY==o.offsetY&&rw==o.rw&&rh==o.rh&&factor==o.factor&&cells==o.cells&&house==o.house&&count==o.count;}
 bool legal(int x,int y)const{return x>0&&y>0&&x<512&&y<512&&x+y>width&&x-y<width&&y-x<width&&x+y<=width+2*height;}
 void project(double x,double y,double&u,double&v)const{u=(x-y+offsetX)*factor/rw;v=(x+y-offsetY)*factor/rh;}
};
struct Marker {double u=0,v=0;uint32_t color=0;bool building=false;};
struct Terrain {Geometry geo;std::vector<uint32_t> colors;};
struct Frame {std::shared_ptr<const Terrain> terrain;std::vector<Marker> markers;uint32_t tick=0;int frame=-1;};
inline uint32_t terrainColor(int land,int level,bool fog){
 static constexpr uint32_t colors[]={0x66794b,0x85836c,0x285e86,0x70756d,0x999684,0xb7a843,0xb4a47c,0x77734d,0xb4cbd0,0x77766b,0x55584f,0x58703d};
 if(land<0||land>=12)return 0x44483a;uint32_t c=colors[land];int shade=std::clamp(level,0,14)*2;
 int r=std::min(255,int(c>>16)+shade),g=std::min(255,int((c>>8)&255)+shade),b=std::min(255,int(c&255)+shade);if(fog){r=r*2/3;g=g*2/3;b=b*2/3;}return uint32_t(r<<16|g<<8|b);
}
template<class Read> class Sampler {
 Read read_;GameView<Read> view_;Geometry geo_{};std::vector<uint32_t> pointers_;std::shared_ptr<Terrain> pending_;std::shared_ptr<const Terrain> terrain_;
 size_t cursor_=0;int lastFrame_=-1;uint32_t reveal_=0;unsigned revision_=0;
 template<class T>T get(uint32_t a,T fallback={})const{return view_.template get<T>(a,fallback);}
public:
 explicit Sampler(Read r):read_(r),view_(r){}
 void reset(){geo_={};pointers_.clear();pending_.reset();terrain_.reset();cursor_=0;lastFrame_=-1;++revision_;}
 unsigned revision()const{return revision_;}
 bool step(uint32_t reveal,int budget=4096){
  Geometry g;g.width=get<int>(Map+0xF4);g.height=get<int>(Map+0xF8);g.cells=get<uint32_t>(Map+0x13C);g.count=get<int>(Map+0x140);
  g.factor=get<float>(Map+0x1488);g.offsetX=get<int>(Map+0x1490);g.offsetY=get<int>(Map+0x1498);g.rw=get<int>(Map+0x14A4);g.rh=get<int>(Map+0x14A8);g.house=get<uint32_t>(0xA83D4C);
  int frame=get<int>(0xA8ED84,-1);if(!g.valid()||frame<0||get<uint8_t>(0xA8E9A0)!=1||!display::inspect(read_).visible()){reset();return false;}
  if(!g.same(geo_)||frame<lastFrame_||reveal_!=reveal){reset();geo_=g;reveal_=reveal;}lastFrame_=frame;
  if(!pending_){pointers_.resize(g.count);if(!read_(g.cells,pointers_.data(),pointers_.size()*4)){reset();return false;}
   pending_=std::make_shared<Terrain>();pending_->geo=g;pending_->colors.resize(512*512);cursor_=0;}
  size_t end=std::min(pointers_.size(),cursor_+size_t(std::clamp(budget,1,16384)));
  for(;cursor_<end;++cursor_){int x=int(cursor_%512),y=int(cursor_/512);if(!g.legal(x,y))continue;uint32_t p=pointers_[cursor_];
   if(p<0x10000)continue;std::array<uint8_t,0x148> cell{};if(!read_(p,cell.data(),cell.size()))continue;
   int16_t cx=0,cy=0;int land=-1;std::memcpy(&cx,cell.data()+0x24,2);std::memcpy(&cy,cell.data()+0x26,2);std::memcpy(&land,cell.data()+0xEC,4);
   if(cx!=x||cy!=y)continue;int visibility=int8_t(cell[0x120]),fog=int8_t(cell[0x121]);if(!reveal&&visibility!=-1)continue;
   uint32_t color=terrainColor(land,cell[0x11B],!reveal&&fog!=-1);
   if(reveal==2&&(visibility!=-1||fog!=-1))color=(color>>1)&0x7F7F7F;
   pending_->colors[cursor_]=color;
  }
  if(cursor_==pointers_.size()){terrain_=pending_;pending_.reset();++revision_;}return true;
 }
 Frame snapshot(uint32_t now)const{
  Frame out;out.terrain=terrain_;out.tick=now;out.frame=lastFrame_;if(!terrain_)return out;
  for(uint32_t p:view_.vec(0xA8EC78)){int kind=view_.kind(p);if(!view_.live(p)||(kind!=3&&get<uint32_t>(p+0x11C))||get<int>(p+0x220)!=0)continue;
   int x=get<int>(p+0x9C,-1),y=get<int>(p+0xA0,-1);if(x<0||y<0||x>=512*256||y>=512*256)continue;int cx=x/256,cy=y/256;
   if(!geo_.legal(cx,cy)||!terrain_->colors[cy*512+cx])continue;uint32_t owner=get<uint32_t>(p+0x21C);uint8_t rgb[3]{};if(owner<0x10000||!read_(owner+22265,rgb,3))continue;
   Marker m;geo_.project(double(x)/256,double(y)/256,m.u,m.v);if(m.u<0||m.u>=1||m.v<0||m.v>=1)continue;
   m.color=uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2];m.building=kind==3;out.markers.push_back(m);
  }return out;
 }
};
inline void raster(const Terrain&t,int w,int h,std::vector<uint32_t>&pixels){
 if(w<=0||h<=0||w>4096||h>4096){pixels.clear();return;}pixels.assign(size_t(w)*h,0x101710);if(!t.geo.valid()||t.colors.size()!=512*512)return;const auto&g=t.geo;
 for(int py=0;py<h;++py)for(int px=0;px<w;++px){double u=(px+.5)*g.rw/(w*g.factor)-g.offsetX,v=(py+.5)*g.rh/(h*g.factor)+g.offsetY;
  int x=int(std::floor((u+v)/2)),y=int(std::floor((v-u)/2));if(g.legal(x,y)){uint32_t c=t.colors[y*512+x];pixels[size_t(py)*w+px]=c?c:0x080c08;}}
}
}}
