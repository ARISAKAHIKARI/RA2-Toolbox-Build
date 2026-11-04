#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_navigation.h"
#include "../../external_reader/source/radar_publication.h"
#include "../../external_reader/source/scene_bank.h"
#include "../source/minimap_viewport.h"
#include <cassert>
#include <cstring>
#include <iostream>

namespace {
using namespace external;
constexpr uint32_t Cells=0x100000,Cell=0x200000,House=0x300000,Tile=0x400000;
std::vector<uint8_t> memory(0x1000000);size_t reads=0;
bool read(uint32_t p,void*out,size_t n){++reads;if(uint64_t(p)+n>memory.size())return false;std::memcpy(out,memory.data()+p,n);return true;}
template<class T>void put(uint32_t p,const T&value){std::memcpy(memory.data()+p,&value,sizeof value);}
void publish(const radar::Geometry&g){
 put(radar::Map+0xF4,g.width);put(radar::Map+0xF8,g.height);put(radar::Map+0xFC,g.bounds.left);put(radar::Map+0x100,g.bounds.top);put(radar::Map+0x104,g.bounds.columns);put(radar::Map+0x108,g.bounds.rows);
 put(radar::Map+0x13C,g.cells);put(radar::Map+0x140,g.count);put(radar::Map+0x1488,g.factor);put(radar::Map+0x1490,g.offsetX);put(radar::Map+0x1498,g.offsetY);
 put(radar::Map+0x14A4,g.rw);put(radar::Map+0x14A8,g.rh);put(radar::Map+0x1240,g.rawWidth);put(radar::Map+0x1244,g.rawHeight);
}
struct Assets {
 struct ActorArt{};static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 uint64_t revision(){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&i){i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xffa03020);i.depth.assign(1800,0);return true;}
 bool shroud(bool,int,voxel::Image&){return false;}
 bool overlay(uint32_t,uint32_t,int,voxel::Image&,bool=false){return false;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const visual::Pose&,voxel::Image&){return false;}
 bool convertPalette(uint32_t,std::array<uint32_t,256>&){return false;}
 bool spritePalette(uint32_t,const std::string&,bool,uint32_t,std::array<uint32_t,256>&){return false;}
 bool particlePalette(std::array<uint32_t,256>&){return false;}
 bool debrisPalette(uint32_t,std::array<uint32_t,256>&){return false;}
 std::vector<std::string> spriteNames(uint32_t,uint32_t,bool){return {};}
 bool resourceSprite(uint32_t,uint32_t,const std::array<uint32_t,256>&,int,voxel::Image&,bool,bool){return false;}
 bool overlayDepthShape(voxel::Image&,unsigned,bool){return false;}
 int resourceFrames(uint32_t,uint32_t,bool){return 0;}
};
radar::Frame sample(radar::Sampler<decltype(&read)>&sampler,bool overview){
 const auto revision=sampler.revision();
 for(int n=0;n<64;++n){assert(sampler.step(1,4096,true,true,false,overview));if(sampler.hasTerrain()&&sampler.revision()!=revision)return sampler.snapshot(100,10);}
 assert(false);return {};
}
}
int main(){
 using namespace external;
 radar::Geometry native{64,64,59,69,120,114,1.f,Cells,House,512*512,{64,64,2,2,60,56},120,114};
 assert(native.valid());const auto unchanged=native;radar::Geometry overview;
 assert(radar::tacticalOverviewProjection(native,overview)&&native.same(unchanged));
 assert(overview.offsetX==60&&overview.offsetY==63&&overview.rawWidth==120&&overview.rawHeight==121&&overview.projectionAdjusted);
 assert(overview.sourceWidth()==120&&overview.sourceHeight()==114&&overview.sourceOffsetX()==59&&overview.sourceOffsetY()==69);
 radar::Geometry again;assert(radar::tacticalOverviewProjection(overview,again)&&again.same(overview));
 // Every normal-sized viewport reaches both camera edges, including odd
 // dimensions. Native Radar's smaller crop would clip the upper strip.
 for(auto size:{std::pair{1280,720},std::pair{1279,719},std::pair{2400,1440}}){
  const auto extent=native.bounds.cameraExtent();double x=-100000,y=-100000;
  assert(native.bounds.clampCamera(x,y,size.first,size.second));
  assert(x-size.first/2==extent.left&&y-size.second/2==extent.top);
  assert((x-size.first/2)/30.+overview.offsetX==0&&(y-size.second/2)/15.-overview.offsetY==0);
  mainview::View view;view.scenario=0x600000;view.house=House;view.tactical=0x700000;view.frame=10;
  view.screenW=size.first+160;view.screenH=size.second;view.width=size.first;view.height=size.second;
  view.cameraX=int(x)-size.first/2;view.cameraY=int(y)-size.second/2;radargate::Mark mark;
  assert(assist::minimapview::rectangle(overview,view,506,275,mark)&&mark.left==1&&mark.top==1);
  int targetX=0,targetY=0;assert(assist::minimapview::navigate(overview,view,1,1,506,275,targetX,targetY)&&targetX==x&&targetY==y);
  x=y=100000;assert(native.bounds.clampCamera(x,y,size.first,size.second));
  assert((x-size.first/2+size.first)/30.+overview.offsetX==overview.rawWidth);
  assert((y-size.second/2+size.second)/15.-overview.offsetY==overview.rawHeight);
  view.cameraX=int(x)-size.first/2;view.cameraY=int(y)-size.second/2;
  assert(assist::minimapview::rectangle(overview,view,506,275,mark)&&mark.right==505&&mark.bottom==274);
  assert(assist::minimapview::navigate(overview,view,505,274,506,275,targetX,targetY)&&targetX==x&&targetY==y);
 }
 publish(native);put(0xA83D4C,House);put(0xA8ED84,10);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(0xA8ED2C,0x500000u);put(0xA8ED30,1);put(0xA8ED38,1);put(0x500000,Tile);put(Tile+0x2F0,1);
 // An explored, physical north-edge tile is visible to Tactical but sits
 // wholly above the native raw crop. Exercise actual Sampler/Renderer pixels.
 constexpr size_t index=33*512+32;put(Cells+index*4,Cell);put(Cell+0x24,int16_t(32));put(Cell+0x26,int16_t(33));put(Cell+0x11B,int8_t(2));
 put(Cell+0xEC,2);put(Cell+0x44,-1);put(Cell+0x48,-1);put(Cell+0x12C,uint8_t(0x10));put(Cell+0x140,uint8_t(1));
 radar::Sampler<decltype(&read)> nativeSampler(&read),privateSampler(&read);nativeSampler.effects(false,false);privateSampler.effects(false,false);
 auto rawFrame=sample(nativeSampler,false),privateFrame=sample(privateSampler,true);assert(rawFrame.terrain&&privateFrame.terrain);
 assert(rawFrame.terrain->geo.same(native)&&privateFrame.terrain->geo.same(overview));
 assert(privateFrame.terrain->art[index].type==Tile&&privateFrame.terrain->art[index].sourceColor);
 Assets assets;radar::SceneRenderer<Assets> rawRenderer,overviewRenderer;
 auto rawImage=rawRenderer.render(rawFrame,360,171,&read,assets),image=overviewRenderer.render(privateFrame,360,363,&read,assets);
 assert(rawImage&&image);
 assert(std::count(rawImage->pixels.begin(),rawImage->pixels.end(),0xffa03020u)==0);
 assert(std::count(image->pixels.begin(),image->pixels.end(),0xffa03020u)>0);
 assert(std::find(image->pixels.begin(),image->pixels.begin()+image->width,0xffa03020u)!=image->pixels.begin()+image->width);
 assert(radar::sameWorld(privateFrame,radar::publicationContext(&read)));
 auto world=std::make_shared<const radar::Frame>(rawFrame);radar::SceneBank bank;bank.publish(1,1,true,world);
 const auto before=reads;assert(!privateSampler.radarProjectionCompatible(*world,true)&&privateSampler.radarProjectionCompatible(privateFrame,true)&&reads==before);
 assert(bank.get(1,1,1,100,true)==world&&world->terrain->geo.same(native));
 // Same-frame trigger changes must invalidate the private domain even when
 // the native RGB allocation and physical Cell array remain unchanged.
 native.bounds={64,64,12,18,24,20};native.offsetX=39;native.offsetY=101;native.rw=48;native.rh=42;publish(native);
 assert(!radar::sameWorld(privateFrame,radar::publicationContext(&read)));
 auto cropped=sample(privateSampler,true);const auto&g=cropped.terrain->geo;
 assert(g.offsetX==40&&g.offsetY==95&&g.rawWidth==48&&g.rawHeight==49);
 assert(g.sourceWidth()==120&&g.sourceHeight()==114&&g.sourceOffsetX()==39&&g.sourceOffsetY()==101&&radar::sameWorld(cropped,radar::publicationContext(&read)));
 assert(bank.get(1,1,1,100,true)==world&&world->terrain->geo.same(unchanged));
 auto wrong=overview;wrong.bounds=native.bounds;assert(!privateSampler.radarProjectionCompatible(radar::Frame{std::make_shared<radar::Terrain>(radar::Terrain{wrong})},true));
 std::cout<<"PASS complete Tactical overview: camera top/bottom/odd extents, native source identity, previously cropped top terrain pixels, private world bank, same-frame trigger projection and compatibility\n";
}
