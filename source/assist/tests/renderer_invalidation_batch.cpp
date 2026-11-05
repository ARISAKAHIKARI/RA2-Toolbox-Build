#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include RA2_REFERENCE_SCENE
#include <cassert>
#include <chrono>
#include <cstdio>
using namespace external;
struct Meter{uint64_t reads=0;};
static std::vector<uint8_t> memory(0x1000000);
struct Read{
 Meter*reader;
 bool operator()(uint32_t p,void*out,size_t n)const{++reader->reads;if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}
 bool readFresh(uint32_t p,void*out,size_t n){return (*this)(p,out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
};
using Base=cameraassets::Context<Read>;
struct Assets:Base{
 using Base::Base;uint64_t lightRevision=1;bool paletteChanged=false,paletteFail=false;
 struct ActorArt{bool voxel=true;uint32_t type=0;struct FacesView{size_t size()const{return 1;}};struct Geometry{FacesView faces;};Geometry geometry()const{return {};}};
 static size_t cost(const ActorArt&){return 100;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xffffffff);return true;}
 bool voxelLightingAvailable()const{return true;}uint64_t voxelLightingRevision()const{return lightRevision;}
 bool captureActor(uint32_t,uint32_t type,uint32_t,ActorArt&out,const visual::Pose*,int,uint32_t){out.type=type;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool refreshActorPalette(ActorArt&,bool&changed){changed=paletteChanged;return !paletteFail;}
 bool renderSharedActor(const ActorArt&art,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out){
  auto image=std::make_shared<voxel::Image>();image->width=image->height=8;
  image->pixels.assign(64,0xff000000u|uint32_t((art.type*13+pose.frame*103+lightRevision*7103+paletteChanged*77)&0xffffff));out=image;return true;
 }
};
static radar::Frame frame(){auto t=std::make_shared<radar::Terrain>();auto&g=t->geo;g.width=g.height=64;g.offsetX=g.offsetY=64;g.rw=g.rh=g.rawWidth=g.rawHeight=128;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=512*512;g.bounds={64,64,0,0,64,64};
 t->reveal=true;t->colors.resize(512*512);t->baseColors.resize(512*512);t->art.resize(512*512);t->colors[5*512+5]=t->baseColors[5*512+5]=0x193347;
 radar::Frame f;f.terrain=t;f.frame=1;f.tick=1;f.markerScanValid=f.posesPrepared=true;
 for(unsigned n=0;n<256;++n){radar::Marker m;m.kind=2;m.object=0x500000+n*0x200;m.type=0x600000+n*0x100;m.id=42+n;m.owner=0x400000;m.x=(10+n%16*2)*256;m.y=(10+n/16*2)*256;m.pose.voxel=true;f.markers.push_back(m);}return f;
}
template<class Renderer>static auto render(Renderer&r,radar::Frame&f,Read read,Assets&a){return r.render(f,160,80,read,a,{},1,false,true,false,true);}
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(){
 Meter oldMeter,newMeter;Read oldRead{&oldMeter},newRead{&newMeter};Assets oldAssets(oldRead),newAssets(newRead);
 radar::BeforeSceneRenderer<Assets> before;radar::SceneRenderer<Assets> after;before.outputSized(true);after.outputSized(true);auto f=frame();
 unsigned comparisons=0;double oldMs=0,newMs=0;
 for(unsigned cycle=0;cycle<8;++cycle){
  // Retain three generations below the existing 1024-pose trim threshold.
  for(int pose=0;pose<3;++pose){for(auto&m:f.markers)m.pose.frame=pose;f.frame++;f.tick+=3000;
   auto a=render(before,f,oldRead,oldAssets),b=render(after,f,newRead,newAssets);assert(a&&b&&a->pixels==b->pixels);++comparisons;}
  ++oldAssets.lightRevision;++newAssets.lightRevision;oldAssets.paletteChanged=newAssets.paletteChanged=(cycle&1)!=0;
  ++f.frame;f.tick+=3000;auto at=now();auto a=render(before,f,oldRead,oldAssets);oldMs+=now()-at;at=now();auto b=render(after,f,newRead,newAssets);newMs+=now()-at;
  assert(a&&b&&a->pixels==b->pixels&&after.objectCoverage()==before.objectCoverage());++comparisons;
  const auto&s=after.stageStats();assert(s.invalidationKeys==256&&s.invalidationVisits<=1024&&s.artWanted==256&&s.artScanned==256);
  assert(s.readCountsKnown&&s.setupReads+s.terrainScanReads+s.maskReads+s.extraReads==s.terrainReads);
  oldAssets.paletteChanged=newAssets.paletteChanged=false;
 }
 // Failed current palettes still reject the candidate, followed by recovery.
 oldAssets.paletteFail=newAssets.paletteFail=true;assert(!render(before,f,oldRead,oldAssets)&&!render(after,f,newRead,newAssets));
 oldAssets.paletteFail=newAssets.paletteFail=false;++f.frame;f.tick+=3000;auto a=render(before,f,oldRead,oldAssets),b=render(after,f,newRead,newAssets);assert(a&&b&&a->pixels==b->pixels);
 printf("PASS %u full SceneRenderer RGBA/coverage comparisons; 256 changed art keys, <=1024 combined invalidation visits; fresh palette failure/recovery; stage read accounting. Revision-changing mean %.6f -> %.6f ms\n",comparisons,oldMs/8,newMs/8);
}
