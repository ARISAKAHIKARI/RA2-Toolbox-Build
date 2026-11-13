#include "../../external_reader/source/radar_live_shroud.h"
#include "../source/special_unit_visibility.h"
#include "../source/shroud_policy.h"
#include "../../shared/source/world_frame.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace external;
namespace {
constexpr uint32_t Cells=0x100000,Ground=0x200000,Clear=0x201000,House=0x300000,Tile=0x400000,Scenario=0x800000;
std::vector<uint8_t>memory(0x1000000);
size_t cellReads=0;bool lookupReadable=true,advanceDuringMask=false;
template<class T>void put(uint32_t p,T v){std::memcpy(memory.data()+p,&v,sizeof v);}
bool bytes(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size()||(!lookupReadable&&p==0x7F4194))return false;
 if(advanceDuringMask&&n==4096&&p>=0x600000&&p<0x700000){put(0xA8ED84,18);advanceDuringMask=false;}
 if(n==0x11D)++cellReads;std::memcpy(out,memory.data()+p,n);return true;}
struct LiveRead {bool operator()(uint32_t p,void*out,size_t n)const{return bytes(p,out,n);}bool fresh(uint32_t p,void*out,size_t n)const{return bytes(p,out,n);}};
bool shape(bool,int f,voxel::Image&i){i={};i.canvasWidth=i.width=60;i.canvasHeight=i.height=30;i.x=-30;i.y=-15;i.indices.assign(1800,0xFE);
 for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs(x-29.5)/30.+std::abs(y-14.5)/15.<1.)i.indices[y*60+x]=f==15?2:127;
 return true;}
struct Assets {
 struct ActorArt{};int loads=0,overlays=0;
 static size_t cost(const ActorArt&){return 1;}uint64_t revision(){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&i){++loads;i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xffa03020);i.depth.assign(1800,0);return true;}
 bool shroud(bool fog,int f,voxel::Image&i){return shape(fog,f,i);}
 bool overlay(uint32_t,uint32_t,int frame,voxel::Image&i,bool=false){++overlays;i={};i.width=i.height=4;i.x=i.y=-2;i.pixels.assign(16,frame?0xffa020a0:0xff20a030);i.depth.assign(16,0);i.writeDepth=false;return true;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const visual::Pose&,voxel::Image&){return false;}
};
void setup(){
 put(radar::Map+0xF4,64);put(radar::Map+0xF8,64);put(radar::Map+0xFC,0);put(radar::Map+0x100,0);put(radar::Map+0x104,64);put(radar::Map+0x108,64);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,512*512);put(radar::Map+0x1488,1.f);
 put(radar::Map+0x1490,4);put(radar::Map+0x1498,124);put(radar::Map+0x14A4,8);put(radar::Map+0x14A8,8);put(radar::Map+0x1240,8);put(radar::Map+0x1244,8);
 put(0xA83D4C,House);put(0xA8B230,Scenario);put(0xA8ED84,10);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(Scenario+0x352C,100);for(int n=0;n<3;++n)put(Scenario+0x3534+n*4,100);
 put(Cells+(64*512+64)*4,Ground);put(Ground+0x24,int16_t(64));put(Ground+0x26,int16_t(64));put(Ground+0x11B,int8_t(2));put(Ground+0xEC,0);put(Ground+0x44,0);
 put(Cells+(63*512+63)*4,Clear);put(Clear+0x24,int16_t(63));put(Clear+0x26,int16_t(63));put(Clear+0x38,-1);put(Clear+0x44,-1);put(Clear+0x12C,uint8_t(0x10));put(Clear+0x140,uint8_t(1));
 put(0xA8ED28+4,0x500000u);put(0xA8ED28+8,1);put(0xA8ED28+16,1);put(0x500000,Tile);
 put(0xA83D80+4,0x500010u);put(0xA83D80+8,1);put(0xA83D80+16,1);put(0x500010,0x700000u);
 for(int n=0;n<256;++n)put(0x7F4194+n,int8_t(n?1:0));
}
}
int main(){
 setup();radar::Frame f;auto terrain=std::make_shared<radar::Terrain>();
 terrain->geo={64,64,4,124,8,8,1,Cells,House,512*512,{64,64,0,0,64,64},8,8};f.terrain=terrain;
 const size_t pixel=40*240+120;Assets assets;put(Clear+0x12C,uint8_t(0x10));
 // Opaque special-unit replacement follows the native destination mask.
 // Reusing a composed pose must still react to exploration in this frame.
 {
  specialoverlay::Visibility visibility;auto original=std::make_shared<voxel::Image>();original->width=240;original->height=120;original->pixels.assign(240*120,0xffc08040);
  const std::shared_ptr<const voxel::Image> source=original;auto image=source;
  Projection projection{-f.terrain->geo.offsetX*30,f.terrain->geo.offsetY*15,240,120};
  assert(visibility.apply(LiveRead{},assets,projection,255,image));assert(image->pixels[pixel]==0xffc08040);
  const auto held=image;image=source;assert(visibility.apply(LiveRead{},assets,projection,255,image)&&image==held);
  put(Clear+0x12C,uint8_t(0));image=source;assert(visibility.apply(LiveRead{},assets,projection,255,image));assert(image->pixels[pixel]==0);
  image=source;assert(visibility.apply(LiveRead{},assets,projection,128,image));assert(image->pixels[pixel]==0xff624221);
  assert(original->pixels[pixel]==0xffc08040); // never modify the native art or a published image
  auto unreadable=[](uint32_t,void*,size_t){return false;};image=source;
  assert(visibility.apply(unreadable,assets,projection,0,image)&&image==source);
  assert(!visibility.apply(unreadable,assets,projection,255,image)&&image==source);
  put(Clear+0x12C,uint8_t(0x10));
  worldframe::Snapshot sealed;worldframe::Key key{};key.frame=12;sealed.reset(key);
  for(uint32_t address=0x10000;address<memory.size();address+=4096)if(address!=0x7F4000)assert(sealed.prefetch(bytes,address,4096));assert(sealed.seal());
  image=source;assert(!visibility.apply([&](uint32_t p,void*out,size_t n){return sealed.read(p,out,n);},assets,projection,255,image));assert(image==source);
  assert(assist::shroudpolicy::overlayOpacity(0)==255&&assist::shroudpolicy::overlayOpacity(1)==128&&assist::shroudpolicy::overlayOpacity(7)==128);
  for(int strategy:{2,3,4,8})assert(assist::shroudpolicy::overlayOpacity(strategy)==0);
  std::puts("PASS special reveal: native per-pixel black/half/clear visibility, same-frame exploration, immutable compose cache and sealed missing-page rejection");
 }
}
