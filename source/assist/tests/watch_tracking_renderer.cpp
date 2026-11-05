#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#include "../../external_reader/source/camera_assets.h"
#include "../source/watch_scene.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace external;
// The existing controlled terrain/assets boundary is shared verbatim; all
// camera decisions and rendering execute the actual production Renderer.
#include "watch_tracking_renderer_support.inc"
static void checked(bool ok,int line){if(!ok){std::fprintf(stderr,"FAIL watch Renderer line=%d\n",line);std::fflush(nullptr);std::exit(1);}}
#undef assert
#define assert(ok) checked(bool(ok),__LINE__)
static uint64_t pixelsHash(const std::vector<uint32_t>&pixels){uint64_t h=1469598103934665603ull;for(auto p:pixels){h^=p;h*=1099511628211ull;}return h;}
struct WatchAssets:Assets {using Assets::Assets;
 bool renderSharedActor(const ActorArt&,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out){
  const uint32_t colors[]={0xff8040e0u,0xff4080e0u,0xffe08040u};auto image=std::make_shared<voxel::Image>(*testSprite(colors[unsigned(std::max(0,pose.frame))%3],24,24));
  // This boundary marker deliberately sits above the synthetic rectangular
  // TMPs. It tests publication/identity, not the real engine's terrain depth.
  image->skipZ=true;image->writeDepth=false;image->nativeDepth.enabled=false;image->indices.clear();out=image;return true;
 }
};
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);xbrzadapter::gpuPresentation(false);auto terrain=makeTerrain();Read read;WatchAssets assets(read);watchscene::Renderer<Read,WatchAssets> renderer(read,assets);
 presentation::Profile profile;profile.gpu=false;profile.motion=false;profile.spriteLighting=false;renderer.features(profile);renderer.process=919;renderer.epoch=929;renderer.revealMap=true;
 auto value=frame(terrain,200,128);value.tick=1000;const auto center=value.markers[20];radar::sceneBank().publish(919,929,true,std::make_shared<const radar::Frame>(std::move(value)));
 auto&t=renderer.tracks[0];t.fixed=true;t.located=true;t.x=center.x;t.y=center.y;t.z=center.z;t.spatialKind=2;
 bool ready=false;for(int warm=0;warm<32&&!ready;++warm){assert(renderer.update(1000));ready=bool(renderer.views[0].scene);}assert(ready&&renderer.views[0].shown&&!renderer.views[0].frame.focus);
 const auto first=renderer.views[0].scene;const double cx=renderer.views[0].frame.cameraX,cy=renderer.views[0].frame.cameraY;
 std::printf("fixed first hash=%llu camera=%.2f,%.2f pixels=%zu fixed=%d target=%u\n",(unsigned long long)pixelsHash(first->pixels),cx,cy,first->pixels.size(),t.fixed,t.target.id);
 for(int n=201;n<=204;++n){auto next=frame(terrain,n,128);next.tick=uint32_t(1000+n-200);radar::sceneBank().publish(919,929,true,std::make_shared<const radar::Frame>(std::move(next)));auto prior=renderer.views[0].scene;bool completed=false;
  // Production has a bounded preparation budget. Observe the same immutable
  // publication again until it completes; never count its retained old scene
  // as a new frame, and never move on while it is still warming.
  for(int warm=0;warm<64&&!completed;++warm){assert(renderer.update(uint32_t(1000+n-200)));completed=renderer.timings.attempted[0]&&renderer.views[0].scene&&renderer.views[0].scene!=prior;}
  assert(completed&&renderer.sharedSource&&renderer.views[0].shown&&renderer.views[0].frame.cameraX==cx&&renderer.views[0].frame.cameraY==cy);std::printf("fixed publication=%d sourceTick=%u hash=%llu prepared=%d attempted=%d reused=%d\n",n,renderer.sourceTick,(unsigned long long)pixelsHash(renderer.views[0].scene->pixels),renderer.timings.prepared,renderer.timings.attempted[0],renderer.timings.reused[0]);}
 assert(renderer.tracks[0].fixed);assert(!renderer.tracks[0].target);assert(renderer.views[0].scene->pixels!=first->pixels);
 // Arm through the real conversion observer, then make ONLY that object's
 // vtable unavailable. Unknown must not bind a successor or freeze the world.
 auto pending=frame(terrain,210,128);pending.tick=1010;std::vector<uint32_t> objects;for(const auto&m:pending.markers)objects.push_back(m.object);
 put(0xA8EC7C,0x750000u);put(0xA8EC80,int(objects.size()));put(0xA8EC88,int(objects.size()));for(size_t i=0;i<objects.size();++i)put(0x750000+uint32_t(i)*4,objects[i]);
 put(center.type,0x7F6218u);put(center.type+0x404,0x270000u);put(0x270000,0x7E4570u);put(0x270EF0,0);put(0x8192B8,1);put(0x819310,1);put(center.object+0xAC,16);put(center.object+0x68C,uint8_t(1));
 t={};t.target=t.spatial={center.object,center.id};t.members={t.target};t.located=true;t.x=center.x;t.y=center.y;t.z=center.z;t.spatialKind=2;watch::follow(read,t);assert(t.transition.armed);
 radar::sceneBank().publish(919,929,true,std::make_shared<const radar::Frame>(std::move(pending)));for(int warm=0;warm<64;++warm){assert(renderer.update(1010));if(renderer.timings.attempted[0]&&renderer.views[0].scene)break;}auto beforeUnknown=renderer.views[0].scene;assert(beforeUnknown&&renderer.timings.attempted[0]);failAt=center.object;
 for(int n=211;n<=212;++n){auto next=frame(terrain,n,128);next.tick=uint32_t(1010+n-210);radar::sceneBank().publish(919,929,true,std::make_shared<const radar::Frame>(std::move(next)));auto prior=renderer.views[0].scene;bool completed=false;for(int warm=0;warm<64&&!completed;++warm){assert(renderer.update(uint32_t(1010+n-210)));completed=renderer.timings.attempted[0]&&renderer.views[0].scene&&renderer.views[0].scene!=prior;}assert(completed&&renderer.views[0].shown&&renderer.tracks[0].target.pointer==center.object&&!renderer.tracks[0].fixed&&!renderer.views[0].frame.center.target);}
 assert(renderer.views[0].scene->pixels!=beforeUnknown->pixels);failAt=0;assert(renderer.update(1013)&&renderer.tracks[0].target.id==center.id&&!renderer.tracks[0].fixed);
 // Closing a fixed camera uses the same empty/suppressed zero-work path.
 renderer.tracks[0]={};renderer.autoSuppressed=true;reads=0;assert(renderer.update(1014)&&reads==0&&!renderer.views[0].shown&&!renderer.views[0].scene);
 // The presentation still requires a world matching its reveal policy; a
 // fixed camera cannot borrow the reveal=true bank for reveal=false output.
 renderer.tracks[0].fixed=true;renderer.tracks[0].located=true;renderer.tracks[0].x=center.x;renderer.tracks[0].y=center.y;renderer.revealMap=false;renderer.update(1015);assert(!renderer.sharedSource);
 std::puts("PASS actual fixed-coordinate Renderer: four fresh common-world publications change pixels at an unchanged camera, no dead-object dereference or frozen bitmap; close zero-work and reveal-policy bank separation preserved. No game/desktop visual test.");
}
