#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>

using namespace external;
namespace {
constexpr size_t Ore=30*512+25;
constexpr uint32_t Cell=0x40000,Scenario=0x60000;
struct Memory {
 int ambient=100,terrainLight=1000;
 template<class T>static bool value(void*out,size_t n,const T&v){if(n!=sizeof(v))return false;std::memcpy(out,&v,n);return true;}
 bool operator()(uint32_t p,void*out,size_t n)const{
  if(p==0xA8B230)return value(out,n,Scenario);
  if(p==Scenario+0x352C)return value(out,n,ambient);
  if(p==Scenario+0x3534){const int tint[3]={100,100,100};if(n!=sizeof tint)return false;std::memcpy(out,tint,n);return true;}
  if(p==Cell+0x104)return value(out,n,uint32_t(1));
  if(p==Cell+0x34)return value(out,n,uint32_t(0));
  if(p==Cell+0x10A){const int16_t light[3]={1000,int16_t(terrainLight),1000};if(n!=sizeof light)return false;std::memcpy(out,light,n);return true;}
  return false;
 }
};
struct Read {Memory*memory;bool operator()(uint32_t p,void*out,size_t n)const{return (*memory)(p,out,n);}};
static voxel::Image rectangle(int w,int h,int x,int y,uint32_t color,float depth=0){
 voxel::Image out;out.width=w;out.height=h;out.x=x;out.y=y;out.pixels.assign(size_t(w)*h,color);out.depth.assign(out.pixels.size(),depth);return out;
}
struct Assets {
 struct ActorArt{};unsigned paletteEpoch=0,tiles=0,overlays=0;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return 1;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xff426839u+paletteEpoch*0x00120b03);return true;}
 bool tile(uint32_t type,unsigned sub,const std::array<uint32_t,256>&pal,voxel::Image&out){
  ++tiles;out=rectangle(60,30,-30,-15,pal[1]+sub*0x00070203);
  for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs(x-29.5)/30.+std::abs(y-14.5)/15.>=1.)out.pixels[size_t(y)*60+x]=0;
  if(type==2){auto cliff=rectangle(130,94,-90,-78,0xff805030,9);for(size_t n=0;n<cliff.pixels.size();++n)if(n%7==0)cliff.pixels[n]=0;out.attachments.push_back(std::move(cliff));}
  return true;
 }
 bool overlay(uint32_t,uint32_t,int frame,voxel::Image&out,bool=false){
  ++overlays;
  if(frame%5==4){out={};return true;} // deletion must clear the old pixels
  const int w=frame%5==1?58:16,h=frame%5==1?42:18;
  out=rectangle(w,h,-w/2-6,-h/2+3,0xffc09320u+unsigned(frame)*0x00070103,3);
  for(size_t n=0;n<out.pixels.size();++n)if(n%5==0)out.pixels[n]=0;
  auto shadow=rectangle(frame%5==2?176:70,frame%5==2?88:25,-90,-35,0x90000000u,0);
  shadow.flat=shadow.shadow=true;shadow.depthBias=-2;shadow.writeDepth=false;out.attachments.push_back(std::move(shadow));
  if(frame%5==3){out.width=out.height=0;out.pixels.clear();out.depth.clear();} // attachment-only image
  return true;
 }
 bool shroud(bool,int,voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return true;}
 bool renderActor(const ActorArt&,const visual::Pose&,voxel::Image&out){out=rectangle(45,37,-22,-30,0xffc050a0,5);return true;}
};
static std::shared_ptr<radar::Terrain> makeTerrain(){
 auto out=std::make_shared<radar::Terrain>();out->geo={32,32,32,32,64,64,1,0x10000,0x20000,512*512,{32,32,0,0,32,32},64,64};out->reveal=true;
 out->colors.resize(512*512);out->baseColors.resize(512*512);out->art.resize(512*512);out->shade.resize(512*512);
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(out->geo.legal(x,y)){size_t n=size_t(y)*512+x;out->colors[n]=0x304820;out->art[n].type=1;out->art[n].sourceColor=0x304820;}
 out->art[Ore].cell=Cell;out->art[Ore].overlayType=0x50000;
 out->art[31*512+26].type=2;out->art[31*512+26].level=2;
 const size_t hole=29*512+25;out->art[hole]={};out->colors[hole]=0;out->baseColors[hole]=0x193845;
 return out;
}
using Renderer=radar::SceneRenderer<Assets>;
static const radar::SceneRegion Crop{500,200,768,384,true};
static std::shared_ptr<const radar::Scene> draw(Renderer&renderer,radar::Frame&frame,Read read,Assets&assets,double scale,unsigned&calls){
 std::shared_ptr<const radar::Scene> scene;calls=0;
 do{frame.tick+=16;scene=renderer.render(frame,int(Crop.width*scale),int(Crop.height*scale),read,assets,Crop,3,true,true,true);assert(++calls<=32);}while(!scene||!renderer.materialsReady());return scene;
}
static std::shared_ptr<const radar::Scene> full(radar::Frame frame,Read read,Assets assets,double scale){Renderer renderer;unsigned calls;return draw(renderer,frame,read,assets,scale,calls);}
}
int main(){
 uint64_t pixels=0,partialCells=0,fullCells=0;unsigned banks=0;
 for(double scale:{1.,1.5}){
  Memory memory;Read read{&memory};Assets assets;Renderer renderer;radar::Frame frame;frame.terrain=makeTerrain();frame.tick=100;frame.frame=10;
  radar::Marker marker;marker.x=25*256;marker.y=30*256;marker.object=0x70000;marker.type=3;marker.id=1;marker.owner=0x80000;marker.kind=2;frame.markers.push_back(marker);
  unsigned calls;auto original=draw(renderer,frame,read,assets,scale,calls);auto before=renderer.nativeStats();fullCells+=before.cells;assert(before.cells>1000&&before.partialBanks==0);
  unsigned changedOutput=0;for(int phase=1;phase<=5;++phase){
   auto next=std::make_shared<radar::Terrain>(*frame.terrain);next->art[Ore].overlayFrame=uint8_t(phase);frame.terrain=next;
   auto actual=draw(renderer,frame,read,assets,scale,calls);auto after=renderer.nativeStats();auto expected=full(frame,read,assets,scale);
   assert(calls==1&&after.partialBanks==before.partialBanks+1&&after.cells-before.cells<before.cells/4);
   assert(actual->pixels==expected->pixels);changedOutput+=actual->pixels!=original->pixels;pixels+=actual->pixels.size();partialCells+=after.cells-before.cells;++banks;before=after;
  }
  assert(changedOutput>=2);
  if(scale!=1.)continue;
  // Source identity/geometry/color changes and current evaluated lighting
  // must take the complete reconstruction path rather than applying a stale
  // single-page result. These are independent of overlay-frame animation.
  const std::vector<std::function<void(radar::Terrain&)>> mutate{
   [](auto&t){++t.art[Ore].sub;},[](auto&t){++t.art[Ore].level;},[](auto&t){t.art[Ore].type=2;},
   [](auto&t){t.art[Ore].sourceColor^=0x101010;},[](auto&t){++t.art[Ore].overlayType;},
   [](auto&t){t.art[Ore].overlaySourceType=0x51000;},[](auto&t){++t.art[Ore].overlay;},
   [](auto&t){t.baseColors[29*512+25]^=0x113355;}
  };
  for(const auto&change:mutate){auto next=std::make_shared<radar::Terrain>(*frame.terrain);++next->art[Ore].overlayFrame;change(*next);frame.terrain=next;
   auto actual=draw(renderer,frame,read,assets,scale,calls);auto after=renderer.nativeStats();assert(after.partialBanks==before.partialBanks&&after.cells-before.cells>1000);assert(actual->pixels==full(frame,read,assets,scale)->pixels);pixels+=actual->pixels.size();before=after;}
  for(unsigned change=0;change<3;++change){auto next=std::make_shared<radar::Terrain>(*frame.terrain);++next->art[Ore].overlayFrame;frame.terrain=next;
   if(change==0)memory.terrainLight=550;else if(change==1)memory.ambient=75;else ++assets.paletteEpoch;
   auto actual=draw(renderer,frame,read,assets,scale,calls);auto after=renderer.nativeStats();assert(after.partialBanks==before.partialBanks&&after.cells-before.cells>1000);assert(actual->pixels==full(frame,read,assets,scale)->pixels);pixels+=actual->pixels.size();before=after;}
 }
 std::printf("PASS actual SceneRenderer partial terrain: %u isolated overlay banks, %llu partial cells / %llu initial full cells, %llu exact output pixels; changing/removed/attachment-only bounds, overlapping cliff/shadow/actor/baseRGB, native+1.5x xBRZ, source/palette/current local/global light guards\n",banks,(unsigned long long)partialCells,(unsigned long long)fullCells,(unsigned long long)pixels);
}
