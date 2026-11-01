#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/viewport_canvas.h"
#include "../../external_reader/source/scene_frame_reuse.h"
#include <cassert>
#include <chrono>
#include <cstdio>
using namespace external;
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static std::shared_ptr<const voxel::Image> image(uint32_t color){
 auto p=std::make_shared<voxel::Image>();p->width=60;p->height=30;p->x=-30;p->y=-15;p->pixels.resize(1800);p->nativeDepth.enabled=true;p->nativeDepth.gradient=nativesurface::Gradient::Flat;
 for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs((x+.5-30)/30)+std::abs((y+.5-15)/15)<=1)p->pixels[y*60+x]=color;
 return p;
}
static void ground(camera::NativeCanvas&out,const std::shared_ptr<const voxel::Image>&tile,int w,int h){
 assert(out.reset(w,h));out.retainGround(true);size_t id=0;
 for(int y=0;y<h;y+=30)for(int x=0;x<w;x+=60){out.underlayRect(x,y,x+60,y+30,0x193347);out.terrainSource(id++,tile,float(x+30),float(y+15),float(y+15),{},0);}
}
static uint64_t hash(const std::vector<uint32_t>&pixels){uint64_t h=1469598103934665603ull;for(auto p:pixels)h=(h^p)*1099511628211ull;return h;}
int main(){
 xbrzadapter::gpuPresentation(false);auto tile=image(0xff80b040),actor=image(0xffee9040);camera::NativeCanvas world;ground(world,tile,6000,3000);
 auto base=std::make_shared<std::vector<uint32_t>>();assert(world.presentRegion(600,300,*base,0,0,6000,3000,1));camera::ViewportCanvas canvas;
 auto prepare=[&](camera::ViewportCanvas&target,const camera::NativeCanvas&source,double x=0,unsigned mode=1,colorprecision::Mode precision=colorprecision::Mode::NativeSteps){return target.prepare(source,*base,600,300,x,0,6000,3000,1,precision,mode,base,{}, {},1);};
 assert(prepare(canvas,world));assert(canvas.recordedGpuFrame()->draws.size()==10000);const auto firstPlan=*canvas.completeGpuPlan();std::vector<uint32_t>first;assert(canvas.finish(first,true));
 const double start=now();for(int n=0;n<80;++n){assert(prepare(canvas,world));assert(camera::samePixels(firstPlan,*canvas.completeGpuPlan()));}const double prepareMs=(now()-start)/80;
#ifdef STATIC_PREFIX_TEST
 assert(canvas.terrainPlanReused());
#endif
 // The cached prefix must discard all previous dynamic calls, effects and fog.
 canvas.blit(*actor,300,300,1,1,300,{},0,0,actor);auto fog=std::make_shared<const std::vector<uint8_t>>(180000,63);std::vector<uint32_t>dynamic;assert(canvas.finish(dynamic,true,fog));assert(dynamic!=first);
 assert(prepare(canvas,world));std::vector<uint32_t>again;assert(canvas.finish(again,true));assert(again==first);
 assert(prepare(canvas,world));canvas.annotationDot(20,30,0xff0000);worldfx::View effectView;effectView.width=6000;effectView.height=3000;effectView.pixelsW=600;effectView.pixelsH=300;gpueffects::Builder effects(effectView);effects.line({20,20},{40,40},0xff00ff);canvas.gpuEffects(effects.finish());canvas.gpuMaskPlan(std::make_shared<const gpushroud::Plan>());vulkancompute::MotionView motion;canvas.motion(motion,true);
 assert(canvas.recordedGpuFrame()->draws.size()==10001&&canvas.recordedGpuFrame()->effects&&canvas.recordedGpuFrame()->maskPlan&&canvas.recordedGpuFrame()->collectGuides);
 assert(prepare(canvas,world));assert(canvas.recordedGpuFrame()->draws.size()==10000&&!canvas.recordedGpuFrame()->effects&&!canvas.recordedGpuFrame()->maskPlan&&!canvas.recordedGpuFrame()->fogAlpha&&!canvas.recordedGpuFrame()->collectGuides);
 std::vector<uint32_t>clean;assert(canvas.finish(clean,true));assert(clean==first);
 // Holding the terrain owner forces COW on a real mutation: same revision,
 // pointer to world and simulation frame must not hide a changed light.
 auto prior=world.fork();lighting::State dark;dark.ambient=400;world.terrainSource(0,tile,30,15,15,dark,1);assert(prepare(canvas,world));
#ifdef STATIC_PREFIX_TEST
 assert(!canvas.terrainPlanReused());
#endif
 camera::ViewportCanvas fresh;assert(prepare(fresh,world));assert(camera::samePixels(*canvas.completeGpuPlan(),*fresh.completeGpuPlan())==false); // independent depth owners
 std::vector<uint32_t>changed,reference;assert(canvas.finish(changed,true));assert(fresh.finish(reference,true));assert(changed==reference&&changed!=first);
 assert(prepare(canvas,prior));std::vector<uint32_t>old;assert(canvas.finish(old,true));assert(old==first);
 // Crop, sampling and precision are part of the prefix, even when the
 // immutable ground owner and caller terrain revision are unchanged.
 for(auto mode:{0u,1u})for(auto precision:{colorprecision::Mode::NativeSteps,colorprecision::Mode::SmoothRGB}){
  assert(prepare(canvas,prior,-50,mode,precision));camera::ViewportCanvas comparison;assert(prepare(comparison,prior,-50,mode,precision));
  std::vector<uint32_t>a,b;assert(canvas.finish(a,true));assert(comparison.finish(b,true));assert(a==b);
 }
 // An unrelated invalid dynamic draw cannot poison the retained terrain.
 assert(prepare(canvas,prior));voxel::Image invalid=*actor;invalid.pixels.clear();canvas.blit(invalid,300,300,1,1,300,{},0,0);assert(!canvas.completeGpuPlan());assert(prepare(canvas,prior));assert(canvas.completeGpuPlan());
 canvas.reset();assert(prepare(canvas,prior));assert(canvas.recordedGpuFrame()->draws.size()==10000);
 std::printf("PASS actual ViewportCanvas/GpuSceneRecord: 10000 TMP commands; CPU pixels/depth; dynamic tail removal; fog reset; same-frame light/COW identity; crop/mode/precision; invalid-tail recovery. prepare_mean_ms=%.6f output_hash=%llu\n",prepareMs,(unsigned long long)hash(first));
}
