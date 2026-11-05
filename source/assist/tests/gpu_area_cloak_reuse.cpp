#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/object_canvas.h"
#include "../../external_reader/source/vulkan_area.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Executes the production Vulkan pipeline. A second persistent backend is
// forced cold by changing only page revisions; these host-only cache tokens
// never enter shader pixel arithmetic. Every resulting output word is equal.
using namespace external;
using Draw=camera::ObjectCanvas::Draw;
static uint64_t frames=0,words=0;
static std::shared_ptr<voxel::Image> image(int w,int h,uint32_t color,bool cloak=false){
 auto p=std::make_shared<voxel::Image>();p->width=w;p->height=h;p->pixels.resize(size_t(w)*h);
 p->nativeDepth.enabled=true;p->nativeDepth.voxel=cloak;p->nativeDepth.gradient=nativesurface::Gradient::Flat;
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)p->pixels[size_t(y)*w+x]=color^uint32_t((x*3+y*7)&31);
 return p;
}
static Draw object(std::shared_ptr<const voxel::Image>p,float x,float y,int offset=0,bool cloak=false){
 Draw d;d.image=std::move(p);d.x=x;d.y=y;d.base=100;d.lift=0;
 d.material.nativeZ=true;d.material.displace=cloak;d.material.cloakWords=offset;return d;
}
static camera::NativeCanvas ground(int w,int h){
 camera::NativeCanvas c;assert(c.reset(w,h));auto tile=image(64,32,0xff408060);size_t id=0;
 for(int y=0;y<h;y+=32)for(int x=0;x<w;x+=64)c.terrainSource(id++,tile,float(x),float(y),0,{},0);
 return c;
}
struct Compare {
 vulkancompute::AreaResolve warm,cold;uint64_t epoch=0;bool required=false;
 std::vector<uint32_t>latest;
 explicit Compare(bool need):required(need){}
 bool check(const gpuarea::Plan&p,const char*name){
  const auto started=std::chrono::steady_clock::now();
  std::vector<uint32_t>actual;if(!warm.render(p,actual)){
   std::fprintf(stderr,"VULKAN_UNAVAILABLE %s: %s\n",name,warm.diagnostics().reason.c_str());
   if(required)std::exit(1);return false;
  }
  auto reference=p;++epoch;
  for(auto&page:reference.pages)page.version^=(epoch<<32);
  std::vector<uint32_t>expected;if(!cold.render(reference,expected)){
   std::fprintf(stderr,"cold render failed: %s\n",cold.diagnostics().reason.c_str());std::abort();
  }
  assert(cold.prefixBuilds()==p.pages.size());
  if(actual!=expected){size_t n=0;while(n<actual.size()&&n<expected.size()&&actual[n]==expected[n])++n;
   std::fprintf(stderr,"GPU cold/warm mismatch %s at %zu, actual=%08x expected=%08x\n",name,n,n<actual.size()?actual[n]:0,n<expected.size()?expected[n]:0);std::abort();
  }
  assert(warm.residentComparisons()<=p.pages.size());
  assert(warm.diagnostics().readbacks==1&&cold.diagnostics().readbacks==1);
  ++frames;words+=actual.size();latest=std::move(actual);
  std::printf("CASE %s pages=%zu rebuild=%llu comparisons=%llu uploads=%llu bytes=%llu total_ms=%.3f\n",name,p.pages.size(),(unsigned long long)warm.prefixBuilds(),(unsigned long long)warm.residentComparisons(),(unsigned long long)warm.diagnostics().uploads,(unsigned long long)warm.diagnostics().uploadedBytes,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
  return true;
 }
};
static bool small(bool required){
 auto base=ground(517,333);auto body=image(48,48,0xffbb8844),cloak=image(49,43,0xff5070a0,true);
 std::vector<Draw>draws{object(body,105,115),object(cloak,127.25f,125.75f,-129,true),object(body,128,120),object(cloak,260,250,517+130,true)};
 camera::ObjectCanvas recorder;Compare compare(required);
 auto plan=[&]{gpuarea::Plan p;assert(recorder.planGPU(base,draws,129,83,517,333,p));assert(p.nativeSurface&&gpuarea::valid(p));return p;};
 auto check=[&](const char*name){return compare.check(plan(),name);};
 if(!check("initial"))return false;
 assert(compare.warm.prefixBuilds()==plan().pages.size());
 check("identical");assert(compare.warm.prefixBuilds()==0);
 draws[1].x+=2;draws[1].y+=1;check("cross-page-move");assert(compare.warm.prefixBuilds()<plan().pages.size());
 draws[0].x+=3;check("earlier-source-writer");draws[2].x+=1;check("later-source-writer");
 for(int offset:{-1,1,-518,518,-1000000,1000000,517*130+100}){
  draws[1].material.cloakWords=offset;check("signed-wrapped-outside");
 }
 draws[1].material.cloakWords=128;draws[3].material.cloakWords=-128;check("chained-cloaks");
 std::swap(draws[0],draws[3]);check("painter-order-change");draws.erase(draws.begin()+1);check("one-cloak-removed");
 for(int scale:{2,3}){auto scaled=std::make_shared<voxel::Image>(*cloak);scaled->rasterScale=scale;scaled->x=-17;scaled->y=-13;
  draws.push_back(object(scaled,127.25f,255.75f,-518,true));check("raster-scale");draws.back().x+=1;check("scaled-move");draws.pop_back();check("scaled-remove");
 }
 auto attached=std::make_shared<voxel::Image>(*cloak);auto part=*cloak;part.x=-30;part.y=5;part.shadow=true;part.airborneShadow=true;
 attached->attachments.push_back(part);attached->shadowsAfter=true;draws.push_back(object(attached,506,123,131,true));draws.back().drop=15;check("attachment");draws.back().material.clipBottom=9;check("clipped-attachment");
 auto alpha=std::make_shared<camera::NativeCanvas::AlphaPages>();(*alpha)[1]=std::make_shared<const std::vector<uint8_t>>(128*128,63);base.nativeAlpha(alpha);check("native-alpha-change");
 // Retained GPU aliases are weak. Expiration of an old non-null alpha must
 // remain distinguishable from a page whose alpha was always absent.
 std::weak_ptr<const std::vector<uint8_t>>oldAlpha=(*alpha)[1];
 base.nativeAlpha(std::shared_ptr<const camera::NativeCanvas::AlphaPages>{});alpha.reset();assert(oldAlpha.expired());
 check("expired-alpha-removal");assert(compare.warm.prefixBuilds()>0);
 auto manual=plan();auto scaledDraws=std::make_shared<std::vector<vulkancompute::DrawJob>>(*manual.objects);
 for(auto&draw:*scaledDraws)if(draw.displace){draw.nativeScaleX=draw.nativeScaleY=2;draw.nativePitch=259;}
 manual.objects=scaledDraws;assert(compare.check(manual,"nonunit-native-scale"));assert(compare.warm.prefixBuilds()==manual.pages.size());
 check("restore-native-scale");
 // A base page used only as a displacement source must invalidate readers.
 camera::NativeCanvas sparse;assert(sparse.reset(517,333));sparse.terrainSource(0,nullptr,0,0,0,{},0);
 auto first=image(64,64,0xff00aa22);first->skipZ=true;sparse.blit(*first,20,20,1,1,0,{},0);
 base=sparse;draws={object(cloak,400,200,20*517+20-(200*517+400),true)};check("source-only-initial");const auto before=compare.latest;
 auto changed=image(64,64,0xffdd3377);changed->skipZ=true;base.blit(*changed,20,20,1,1,0,{},0);check("source-only-change");assert(compare.latest!=before);
 auto missing=plan();auto source=std::find_if(missing.pages.begin(),missing.pages.end(),[](const auto&p){return p.x==0&&p.y==0;});assert(source!=missing.pages.end());missing.pages.erase(source);
 assert(compare.check(missing,"source-only-removal"));assert(compare.warm.prefixBuilds()>0);check("source-only-restored");
 draws.clear();check("last-cloak-removed");draws={object(body,40,40)};check("plain-object-after-cloak");
 return true;
}
static void large(bool required){
 auto base=ground(4096,2048);auto body=image(48,48,0xffbb8844),cloak=image(48,48,0xff5070a0,true);std::vector<Draw>draws;
 for(int n=0;n<64;++n)draws.push_back(object(body,float(80+n%16*240),float(80+n/16*400)));
 draws.push_back(object(cloak,180,180,-129,true));camera::ObjectCanvas recorder;Compare compare(required);gpuarea::Plan p;
 auto plan=[&]{p={};assert(recorder.planGPU(base,draws,512,256,4096,2048,p));assert(p.pages.size()==512);};
 plan();assert(compare.check(p,"large-initial"));assert(compare.warm.prefixBuilds()==512);
 plan();assert(compare.check(p,"large-identical"));assert(compare.warm.prefixBuilds()==0);assert(compare.warm.residentComparisons()==512);
 const auto coldBytes=compare.cold.diagnostics().uploadedBytes;
 for(int n=0;n<3;++n){draws.back().x=float(181+n%2);plan();assert(compare.check(p,"large-moving-cloak"));
  assert(compare.warm.prefixBuilds()>0&&compare.warm.prefixBuilds()<=4);assert(compare.warm.diagnostics().uploadedBytes<coldBytes/4);
 }
 // A global lighting change legitimately invalidates every terrain page.
 // The scoped path must still produce exactly the full-cold GPU image.
 auto tile=base.terrainSources().front().image;lighting::State light;light.intensity=600;size_t id=0;
 for(int y=0;y<2048;y+=32)for(int x=0;x<4096;x+=64)base.terrainSource(id++,tile,float(x),float(y),0,light,0);
 plan();assert(compare.check(p,"large-full-light-change"));assert(compare.warm.prefixBuilds()==512);
}
static void plain(bool required){
 Compare compare(required);gpuarea::Plan p;p.nativeWidth=257;p.nativeHeight=129;p.width=65;p.height=33;p.spanX=257;p.spanY=129;
 auto pixels=std::make_shared<const std::vector<uint32_t>>(gpuarea::PagePixels,0xff207050);
 p.pages={{0,0,pixels},{1,0,pixels},{0,1,pixels},{1,1,pixels}};assert(compare.check(p,"nonnative-shared-owner"));assert(compare.check(p,"nonnative-warm"));assert(compare.warm.prefixBuilds()==0);
 p.pages[1].version=1;assert(compare.check(p,"nonnative-one-dirty"));assert(compare.warm.prefixBuilds()==1);
 p.pages.erase(p.pages.begin());assert(compare.check(p,"nonnative-removal"));
 // Keep page/vector owners and revisions exactly stable across render-mode
 // transitions. Native alpha changes the atlas color, so reusing a slot from
 // the other mode would preserve the wrong already-composed brightness.
 auto manual=p;manual.objects=std::make_shared<const std::vector<vulkancompute::DrawJob>>();
 manual.pages.front().nativeAlpha=std::make_shared<const std::vector<uint8_t>>(gpuarea::PagePixels,63);
 manual.nativeSurface=true;assert(gpuarea::valid(manual));assert(compare.check(manual,"context-native-initial"));assert(compare.warm.prefixBuilds()==manual.pages.size());
 manual.nativeSurface=false;assert(compare.check(manual,"context-native-to-plain"));assert(compare.warm.prefixBuilds()==manual.pages.size());
 manual.nativeSurface=true;assert(compare.check(manual,"context-plain-to-native"));assert(compare.warm.prefixBuilds()==manual.pages.size());
 // Removed pages leave visible holes. A background change therefore changes
 // actual output while preserving every present page owner/revision.
 const auto beforeBackground=compare.latest;manual.background=0xffa02030;
 assert(compare.check(manual,"context-background-change"));assert(compare.warm.prefixBuilds()==manual.pages.size());assert(compare.latest!=beforeBackground);
}
int main(int argc,char**argv){const bool required=argc>1&&std::strcmp(argv[1],"--require")==0;
 if(!small(required))return 0;large(required);plain(required);
 std::printf("PASS actual Vulkan area cloak reuse frames=%llu exact_RGBA_words=%llu; warm/cold identity, dependency mutation/removal, signed offsets, raster scales, native fallback, shared owners and full-light invalidation\n",(unsigned long long)frames,(unsigned long long)words);
}
