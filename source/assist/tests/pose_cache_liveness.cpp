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
 using Base::Base;
 struct ActorArt{bool voxel=true,transient=false;struct FacesView{size_t size()const{return 1;}};struct Geometry{FacesView faces;};Geometry geometry()const{return {};}};
 static size_t cost(const ActorArt&){return 100;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xffffffff);return true;}
 bool voxelLightingAvailable()const{return true;}uint64_t voxelLightingRevision()const{return 1;}
 bool captureActor(uint32_t,uint32_t type,uint32_t,ActorArt&out,const visual::Pose*,int,uint32_t){out.voxel=type!=0x700000;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&out){out.transient=true;return true;}
 bool refreshActorPalette(ActorArt&,bool&changed){changed=false;return true;}
 bool renderSharedActor(const ActorArt&art,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out){
  // A time-budgeted/temporarily unavailable SHP and optional transient VXL
  // legitimately keep their old displayed image. Retiring either is wrong.
  if((!art.voxel||art.transient)&&pose.frame>0)return false;
  auto image=std::make_shared<voxel::Image>();image->width=image->height=512;
  image->pixels.assign(512*512,pose.frame?0xffff0000:0xffffffff);out=std::move(image);return true;
 }
};
static radar::Frame frame(unsigned actors=72){auto t=std::make_shared<radar::Terrain>();auto&g=t->geo;g.width=g.height=4;g.offsetX=g.offsetY=4;g.rw=g.rh=g.rawWidth=g.rawHeight=8;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=4096;g.bounds={4,4,0,0,4,4};
 t->reveal=true;t->colors.resize(512*512);t->baseColors.resize(512*512);t->art.resize(512*512);t->colors[5*512+5]=t->baseColors[5*512+5]=0x193347;
 radar::Frame f;f.terrain=t;f.frame=1;f.tick=1;f.markerScanValid=f.posesPrepared=true;
 for(unsigned n=0;n<actors;++n){radar::Marker m;m.kind=2;m.object=0x500000+n*0x200;m.type=0x600000+n*0x100;m.id=42+n;m.owner=0x400000;m.x=m.y=5*256;m.pose.voxel=true;f.markers.push_back(m);}return f;
}
constexpr size_t PoseCost=512*512,Actors=72,WorkingSet=Actors*PoseCost;
static_assert(WorkingSet<rendermemory::PosePixels&&2*WorkingSet>rendermemory::PosePixels,"test needs individually valid frames and oversized transition union");
using Renderer=radar::SceneRenderer<Assets>;
static std::shared_ptr<const radar::Scene> render(Renderer&r,radar::Frame&f,Read read,Assets&assets){return r.render(f,120,60,read,assets,{},1,false,true,false,true);}
int main(){Read read;
 {
  Assets assets(read);Renderer r;r.outputSized(true);auto oldFrame=frame();auto old=render(r,oldFrame,read,assets);assert(old&&r.objectCoverage().second==Actors);const auto oldPixels=old->pixels;
  auto next=oldFrame;for(auto&m:next.markers)m.pose.frame=1;next.frame=2;next.tick=3000;
  auto current=render(r,next,read,assets);assert(current&&current->pixels!=oldPixels&&old->pixels==oldPixels&&r.objectCoverage().second==Actors);
  for(unsigned retry=0;retry<64;++retry){++next.frame;next.tick+=16;auto same=render(r,next,read,assets);assert(same&&same->pixels==current->pixels&&r.stageStats().failure==radar::RenderFailure::None&&r.objectCoverage().second==Actors);}
  Assets freshAssets(read);Renderer fresh;fresh.outputSized(true);auto control=render(fresh,next,read,freshAssets);assert(control&&control->pixels==current->pixels&&fresh.objectCoverage().second==Actors);
 }
 {
  Assets assets(read);Renderer r;r.outputSized(true);auto old=frame();auto shp=old.markers.front();shp.object=0x580000;shp.type=0x700000;shp.id=500;shp.pose.voxel=false;old.markers.push_back(shp);
  auto transient=shp;transient.object+=0x400;transient.type+=0x400;transient.id+=1;transient.kind=5;transient.pose.voxel=true;old.markers.push_back(transient);
  assert(render(r,old,read,assets)&&r.objectCoverage().second==Actors+2);
  auto next=old;for(auto&m:next.markers)m.pose.frame=1;next.markers.front().pose.frameFrom=0;next.tick=3000;next.frame=2;
  assert(render(r,next,read,assets)&&r.objectCoverage().second==Actors+2);
  ++next.frame;next.tick+=16;assert(render(r,next,read,assets)&&r.objectCoverage().second==Actors+2);
  // Current ordinary VXL + explicitly required old frameFrom + SHP fallback
  // + optional transient VXL fallback all remain. Empty attempted keys cost 0.
  assert(r.stageStats().posePixels==(Actors+3)*PoseCost);
  int x,y,w,h;assert(r.feedbackBounds(next.markers[Actors],x,y,w,h)&&w==512&&h==512);
  assert(r.feedbackBounds(next.markers[Actors+1],x,y,w,h)&&w==512&&h==512);
 }
 {
  Assets assets(read);Renderer r;r.outputSized(true);auto tooLarge=frame(unsigned(rendermemory::PosePixels/PoseCost+1));
  assert(!render(r,tooLarge,read,assets)&&r.stageStats().failure==radar::RenderFailure::VoxelPoseBudget);
  const auto&s=r.stageStats();assert(s.posePixels-s.replacementPixels+s.newPixels>rendermemory::PosePixels);
 }
 {
  Assets assets(read);Renderer r;r.prepareWorld(true);auto old=frame();assert(render(r,old,read,assets));auto published=r.preparedWorld();assert(published&&published->instances.size()==Actors);
  auto retained=published->instances.front().image;assert(retained&&retained->pixels.front()==0xffffffff);
  auto next=old;for(auto&m:next.markers)m.pose.frame=1;next.frame=2;next.tick=3000;assert(render(r,next,read,assets));auto current=r.preparedWorld();
  assert(current&&current!=published&&current->instances.size()==Actors&&current->instances.front().image->pixels.front()==0xffff0000);
  assert(published->instances.front().image==retained&&retained->pixels.front()==0xffffffff);
 }
 std::printf("PASS production pose-cache liveness: %zu actors, current=%zu < budget=%zu < two_generations=%zu; first transition plus 64 retries complete and equal fresh renderer; exact current RGBA/old published CPU image; current/frameFrom/SHP/transient fallback preserved; genuine oversized frame still rejected; held world packet images remain immutable.\n",Actors,WorkingSet,rendermemory::PosePixels,WorkingSet*2);
}
