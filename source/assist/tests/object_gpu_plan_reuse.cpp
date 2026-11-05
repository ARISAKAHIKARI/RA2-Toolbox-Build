#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/object_canvas.h"
#include "../../external_reader/source/scene_frame_reuse.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <map>
using namespace external;
static std::shared_ptr<const voxel::Image> image(uint32_t color){auto p=std::make_shared<voxel::Image>();p->width=64;p->height=32;p->pixels.assign(2048,color);p->nativeDepth.enabled=true;p->nativeDepth.gradient=nativesurface::Gradient::Flat;return p;}
static camera::NativeCanvas ground(int w,int h){camera::NativeCanvas out;assert(out.reset(w,h));auto tile=image(0xff408060);size_t id=0;for(int y=0;y<h;y+=32)for(int x=0;x<w;x+=64)out.terrainSource(id++,tile,float(x),float(y),0,{},0);return out;}
static void equal(const gpuarea::Plan&a,const gpuarea::Plan&b){
 assert(a.nativeWidth==b.nativeWidth&&a.nativeHeight==b.nativeHeight&&a.nativeSurface==b.nativeSurface&&a.pages.size()==b.pages.size()&&a.objects&&b.objects&&a.objects->size()==b.objects->size());
 for(size_t n=0;n<a.objects->size();++n)assert(camera::samePixels((*a.objects)[n],(*b.objects)[n]));
 std::map<std::pair<uint32_t,uint32_t>,const gpuarea::Page*>pages;for(const auto&p:b.pages)pages[{p.x,p.y}]=&p;
 for(const auto&p:a.pages){const auto*q=pages.at({p.x,p.y});assert(p.version==q->version&&*p.pixels==*q->pixels&&p.draws==q->draws&&p.nativeAlpha==q->nativeAlpha&&bool(p.depth)==bool(q->depth));if(p.depth)assert(*p.depth==*q->depth);}
}
static void correctness(){
 auto base=ground(1024,768);camera::ObjectCanvas warm;gpuarea::Plan previous;auto actor=image(0xffcc6633);camera::ObjectCanvas::Draw d;d.image=actor;d.x=125.25f;d.y=126.75f;
 std::vector<camera::ObjectCanvas::Draw>draws{d};unsigned cases=0;auto check=[&]{gpuarea::Plan a,b;camera::ObjectCanvas cold;assert(warm.planGPU(base,draws,160,120,1024,768,a));assert(cold.planGPU(base,draws,160,120,1024,768,b));equal(a,b);previous=std::move(a);++cases;};
 check();auto blank=previous.pages[0].pixels;check();
#ifdef EXPECT_STATIC_AREA_PLAN
 assert(previous.pages[0].pixels==blank);
#endif
 for(int n=0;n<16;++n){draws[0].x+=1.5f;check();}
 lighting::State dark;dark.intensity=640;base.terrainSource(0,image(0xff6633cc),0,0,0,dark,1);check();
 base.restrictPaint(0,0,512,512);check();base.restrictPaint(0,0,1024,768);check();
 base.precision(colorprecision::Mode::SmoothRGB);check();base.precision(colorprecision::Mode::NativeSteps);check();
 auto alpha=std::make_shared<camera::NativeCanvas::AlphaPages>();(*alpha)[257]=std::make_shared<const std::vector<uint8_t>>(16384,63);base.nativeAlpha(alpha);check();
 auto invalid=std::make_shared<voxel::Image>(*actor);invalid->pixels.clear();draws[0].image=invalid;gpuarea::Plan rejected;assert(!warm.planGPU(base,draws,160,120,1024,768,rejected));draws[0].image=actor;check();
 // Dynamic-only pages need immutable blank identity too; recoloring replaces
 // it, and neither an invalid tail nor a geometry change may retain old bins.
 assert(base.reset(1024,768,{},0xff123456));check();blank=previous.pages[0].pixels;check();
#ifdef EXPECT_STATIC_AREA_PLAN
 assert(previous.pages[0].pixels==blank);
#endif
 assert(base.reset(1024,768,{},0xff987654));check();assert(previous.pages[0].pixels!=blank);
 std::printf("PASS GPU area host plan: %u cold/warm DrawJobs and page-reference comparisons; dynamic tails, source/light/level COW, crop, precision, alpha, rejected-tail recovery, stable blank identity and background change\n",cases);
}
static void benchmark(int w,int h){auto base=ground(w,h);auto actor=image(0xffaa7733);std::vector<camera::ObjectCanvas::Draw>draws;for(int n=0;n<64;++n){camera::ObjectCanvas::Draw d;d.image=actor;d.x=float(80+n%16*240);d.y=float(80+n/16*400);draws.push_back(d);}
 camera::ObjectCanvas renderer;gpuarea::Plan plan;assert(renderer.planGPU(base,draws,512,256,w,h,plan));double times[32];for(int n=0;n<32;++n){draws[0].x=float(80+n%2);auto at=std::chrono::steady_clock::now();assert(renderer.planGPU(base,draws,512,256,w,h,plan));times[n]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-at).count();}
 std::sort(std::begin(times),std::end(times));std::printf("PLAN %dx%d TMPs=%zu median_ms=%.6f pages=%zu draws=%zu\n",w,h,base.terrainSources().size(),(times[15]+times[16])/2,plan.pages.size(),plan.objects->size());}
int main(){correctness();benchmark(4096,2048);benchmark(8192,4096);}
