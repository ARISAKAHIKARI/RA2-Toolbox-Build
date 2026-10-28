#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_navigation.h"
#include <cassert>
#include <cstring>
#include <iostream>

namespace {
using namespace external;
constexpr uint32_t Cell=0x200000,Cells=0x100000,House=0x300000,Tile=0x400000;
std::vector<uint8_t> memory(0x1000000);
bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;}
template<class T>void put(uint32_t p,T value){std::memcpy(memory.data()+p,&value,sizeof value);}
struct Assets {
 struct ActorArt{};int left=0,top=0,width=60,height=30,loads=0;
 static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 uint64_t revision(){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&i){
  ++loads;i={};i.width=width;i.height=height;i.x=left-30;i.y=top-15;
  i.pixels.assign(size_t(width)*height,0xffa03020);i.depth.assign(i.pixels.size(),0);return true;
 }
 bool shroud(bool,int,voxel::Image&i){
  i={};i.canvasWidth=60;i.canvasHeight=30;i.width=60;i.height=30;i.x=-30;i.y=-15;
  i.indices.assign(1800,2);i.pixels.assign(1800,0);return true;
 }
 bool overlay(uint32_t,uint32_t,int,voxel::Image&,bool=false){return false;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const visual::Pose&,voxel::Image&){return false;}
};
struct Probe {int offsetX,offsetY,level,left,top,width,height;};
radar::Frame sample(const Probe&q,bool reveal,bool real,bool explored=false){
 std::fill(memory.begin(),memory.end(),0);
 put(radar::Map+0xF4,64);put(radar::Map+0xF8,64);put(radar::Map+0xFC,10);put(radar::Map+0x100,10);
 put(radar::Map+0x104,1);put(radar::Map+0x108,1);put(radar::Map+0x13C,Cells);put(radar::Map+0x140,512*512);
 put(radar::Map+0x1488,1.f);put(radar::Map+0x1490,q.offsetX);put(radar::Map+0x1498,q.offsetY);
 put(radar::Map+0x14A4,8);put(radar::Map+0x14A8,8);put(radar::Map+0x1240,8);put(radar::Map+0x1244,8);
 put(0xA83D4C,House);put(0xA8ED84,10);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(Cells+(64*512+64)*4,Cell);put(Cell+0x24,int16_t(64));put(Cell+0x26,int16_t(64));
 put(Cell+0x11B,int8_t(q.level));put(Cell+0x38,0);put(Cell+0x44,0);put(Cell+0xEC,2);
 put(Cell+0x12C,uint8_t(explored?0x10:0));put(Cell+0x140,uint8_t(explored?1:0));
 put(0xA8ED28+4,0x500000u);put(0xA8ED28+8,1);put(0xA8ED28+16,1);put(0x500000,Tile);
 // A stored overlay exists but must not be admitted with the terrain-only
 // fringe. Likewise the diagram/actor colors keep their previous policy.
 put(0xA83D80+4,0x500010u);put(0xA83D80+8,1);put(0xA83D80+16,1);put(0x500010,0x600000u);
 // The genuine selector lookup is validated; this fully shrouded Cell never
 // needs any neighboring clear pixels or fabricated native draw-frame cache.
 for(int n=0;n<256;++n)put(0x7F4194+n,int8_t(n?1:0));
 radar::Sampler<decltype(&read)> sampler(&read);
 for(int n=0;n<64;++n)assert(sampler.step(reveal,4096,real));
 return sampler.snapshot(100);
}
}
int main(){
 using namespace external;
 // Left, right, top, bottom half tiles, a raised southern source outside the
 // flat RGB backing, an extra-image-only intersection, and a fully offscreen
 // decoded sprite. All share exactly the same 240x120 destination bounds.
 constexpr Probe probes[]={{0,124,0,0,0,60,30},{8,124,0,0,0,60,30},
  {4,129,0,0,0,60,30},{4,121,0,0,0,60,30},{4,118,3,0,0,60,30},
  {10,124,0,-60,-30,120,60},{4,130,0,0,0,60,30}};
 size_t cases=0;
 for(const auto&q:probes){
  auto f=sample(q,true,true);assert(f.terrain&&f.terrain->reveal);
  const auto&g=f.terrain->geo;const auto&art=f.terrain->art[64*512+64];
  assert(g.legal(64,64)&&!g.bounds.usable(64,64,q.level));
  assert(g.rawWidth==8&&g.rawHeight==8&&g.nativeViewport().width==240&&g.nativeViewport().height==120);
  assert(f.terrain->colors[64*512+64]==0&&art.cell==Cell&&art.type==Tile&&art.sourceColor!=0&&art.overlayType==0);
  std::vector<uint32_t> diagram;radar::raster(*f.terrain,80,40,diagram);
  auto diagramOnly=sample(q,true,false);std::vector<uint32_t> expectedDiagram;radar::raster(*diagramOnly.terrain,80,40,expectedDiagram);
  assert(diagram==expectedDiagram&&diagramOnly.terrain->art.empty());
  Assets assets;assets.left=q.left;assets.top=q.top;assets.width=q.width;assets.height=q.height;
  radar::SceneRenderer<Assets> renderer;auto image=renderer.render(f,240,120,&read,assets);
  assert(image&&image->width==240&&image->height==120&&assets.loads==1);
  mapbounds::TileOrigin at;assert(g.tileOrigin(64,64,q.level,at));
  size_t expected=0;for(int y=0;y<120;++y)for(int x=0;x<240;++x){
   bool inside=x>=at.x+q.left&&x<at.x+q.left+q.width&&y>=at.y+q.top&&y<at.y+q.top+q.height;
   assert((image->pixels[size_t(y)*240+x]==0xffa03020u)==inside);expected+=inside;
  }
  assert(expected==size_t(std::count(image->pixels.begin(),image->pixels.end(),0xffa03020u)));
  auto stats=renderer.nativeStats();assert(stats.width==240&&stats.height==120);
  int x=0,y=0,dx=0,dy=0;
  assert(radar::navigate(g,40,20,82,42,640,480,x,y));
  assert(radar::navigate(diagramOnly.terrain->geo,40,20,82,42,640,480,dx,dy)&&x==dx&&y==dy);
  // A fully covered source remains absent under the existing native shroud
  // policy, even when its raised/extra pixels could enter the crop.
  auto masked=sample(q,false,true);assert(masked.terrain&&masked.terrain->art[64*512+64].sourceColor==0);
  ++cases;
 }
 // Conservative collection is bounded and still rejects physical illegality.
 auto f=sample(probes[0],true,true);const auto&g=f.terrain->geo;
 assert(!g.terrainSourceCandidate(500,1)&&!g.terrainSourceVisible(500,1,0));
 // A clear source outside the flat backing cannot punch through an unknown
 // destination cell: native SHP masking still runs after source composition.
 auto native=sample(probes[4],false,true,true);
 assert(native.terrain->art[64*512+64].sourceColor!=0&&!native.terrain->geo.flatVisible(64,64));
 Assets assets;radar::SceneRenderer<Assets> renderer;auto masked=renderer.render(native,240,120,&read,assets);
 assert(masked&&assets.loads==1&&std::find(masked->pixels.begin(),masked->pixels.end(),0xffa03020u)==masked->pixels.end());
 std::cout<<"PASS: "<<cases<<" minimap TMP crop cases, four edges, raised/extra-image sources, offscreen art, fixed extent, unchanged diagram/navigation/overlay and native shroud policy\n";
}
