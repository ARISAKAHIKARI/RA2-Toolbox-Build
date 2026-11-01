#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
using namespace external;
static std::vector<uint8_t> memory(0x1000000);
struct Read{bool operator()(uint32_t p,void*out,size_t n)const{if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}bool readFresh(uint32_t p,void*out,size_t n){return (*this)(p,out,n);}bool imagePath(wchar_t*,DWORD*){return false;}};
using Base=cameraassets::Context<Read>;
struct Assets:Base{
 using Base::Base;enum Mode{Good,Missing,Faces,Raster,Null,Incomplete,Pixels,Palette,PaletteTerrain};Mode mode=Good;
 struct ActorArt{bool voxel=true;size_t faces=1;struct FacesView{size_t n;size_t size()const{return n;}};struct Geometry{FacesView faces;};Geometry geometry()const{return {{faces}};}};
 static size_t cost(const ActorArt&){return 100;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xffffffff);return mode!=PaletteTerrain;}
 bool voxelLightingAvailable()const{return true;}uint64_t voxelLightingRevision()const{return 1;}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&out,const visual::Pose*,int,uint32_t){out.faces=mode==Faces?rendermemory::LiveFaces+1:1;return mode!=Missing;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool refreshActorPalette(ActorArt&,bool&changed){changed=false;return mode!=Palette;}
 bool renderSharedActor(const ActorArt&,const visual::Pose&,std::shared_ptr<const voxel::Image>&out){
  assert(failure.empty());if(mode==Raster){failure="mock-current-posture-unavailable";return false;}if(mode==Null)return true;
  auto i=std::make_shared<voxel::Image>();i->width=i->height=1;i->pixels={0xffffffff};i->incomplete=mode==Incomplete;
  if(mode==Pixels){i->width=i->height=8192;i->gpuRaster=std::make_shared<const gpuvoxel::Prepared>();}
  out=std::move(i);return true;
 }
};
static radar::Frame frame(){auto t=std::make_shared<radar::Terrain>();auto&g=t->geo;g.width=g.height=4;g.offsetX=g.offsetY=4;g.rw=g.rh=g.rawWidth=g.rawHeight=8;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=4096;g.bounds={4,4,0,0,4,4};
 t->reveal=true;t->colors.resize(512*512);t->baseColors.resize(512*512);t->art.resize(512*512);t->colors[5*512+5]=t->baseColors[5*512+5]=0x193347;
 radar::Frame f;f.terrain=t;f.frame=1;f.tick=1;f.markerScanValid=f.posesPrepared=true;radar::Marker m;m.kind=2;m.object=0x500000;m.type=0x600000;m.id=42;m.owner=0x400000;m.x=m.y=5*256;m.pose.voxel=true;m.pose.hvaBody=3;m.pose.hvaTurret=4;f.markers.push_back(m);return f;
}
int main(){Read read;const auto f=frame();
 using R=radar::RenderFailure;const std::pair<Assets::Mode,R> cases[]={{Assets::Missing,R::MissingVoxelArt},{Assets::Faces,R::VoxelFaceBudget},{Assets::Raster,R::VoxelRaster},{Assets::Null,R::VoxelImageMissing},{Assets::Incomplete,R::VoxelIncomplete},{Assets::Pixels,R::VoxelPoseBudget}};
 for(const auto&c:cases){Assets a(read);a.mode=c.first;a.failure="STALE";radar::SceneRenderer<Assets>r;r.outputSized(true);auto image=r.render(f,120,60,read,a,{},1,false,true,false,true);const auto&s=r.stageStats();assert(!image&&s.failure==c.second);assert(s.failureObject==0x500000&&s.failureType==0x600000&&s.failureID==42&&s.failureKind==2&&s.candidates==1);assert(s.hvaBody==3&&s.hvaTurret==4&&s.rasterScale==1&&s.voxelLightAvailable);
  if(c.first==Assets::Faces)assert(s.liveFaces>rendermemory::LiveFaces);
  if(c.first==Assets::Pixels)assert(s.posePixels-s.replacementPixels+s.newPixels>rendermemory::PosePixels);
  if(c.first==Assets::Raster)assert(s.assetFailure=="mock-current-posture-unavailable");else assert(s.assetFailure.empty());
  // A failed scene is still rejected. Clearing the simulated problem allows
  // the unchanged production route to build a complete image next time.
  a.mode=Assets::Good;auto next=f;next.tick=3000;next.frame=2;r.reset();assert(r.render(next,120,60,read,a,{},1,false,true,false,true));assert(r.stageStats().failure==R::None);
 }
 Assets a(read);radar::SceneRenderer<Assets>r;r.outputSized(true);assert(r.render(f,120,60,read,a,{},1,false,true,false,true));
 a.mode=Assets::Palette;assert(!r.render(f,120,60,read,a,{},1,false,true,false,true));assert(r.stageStats().failure==R::ActorPalette&&r.stageStats().failureObject==0x500000);
 a.mode=Assets::Good;r.reset();assert(r.render(f,120,60,read,a,{},1,false,true,false,true));a.mode=Assets::PaletteTerrain;assert(!r.render(f,120,60,read,a,{},1,false,true,false,true));assert(r.stageStats().failure==R::TerrainPalette);
 puts("PASS production SceneRenderer diagnostics: exact missing art/face budget/raster/null/incomplete/pixel budget/palette causes, identity and current pose, stale failure cleared, fail-closed behavior and recovery preserved");
}
