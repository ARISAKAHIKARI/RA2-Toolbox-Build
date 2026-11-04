#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#ifdef BEFORE_RENDERER
#include "../../../../diagnostics/renderer-plan-performance/before-scene/radar_scene.h"
#else
#include "../../external_reader/source/radar_scene.h"
#endif
#include <cassert>
#include <cstdio>
using namespace external;
static std::vector<uint8_t> memory(0x1000000);static uint32_t failAt=0;static unsigned revisionReads=0;
template<class T>static void put(uint32_t p,const T&v){std::memcpy(memory.data()+p,&v,sizeof v);}
struct FixtureRead {bool operator()(uint32_t at,void*out,size_t n)const{if(at==0x822CF8)++revisionReads;if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;}bool readFresh(uint32_t at,void*out,size_t n){return (*this)(at,out,n);}bool imagePath(wchar_t*,DWORD*){return false;}};
using BaseAssets=cameraassets::Context<FixtureRead>;
struct Assets:BaseAssets{
 using BaseAssets::BaseAssets;uint32_t color=0xff2266aa;bool indexed=false;
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(color);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&p,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,p[1]);if(indexed)out.indices.assign(1800,1);out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
};
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(){
 FixtureRead read;Assets assets(read);radar::SceneRenderer<Assets> renderer;renderer.outputSized(true);xbrzadapter::gpuPresentation(false);
 auto terrain=std::make_shared<radar::Terrain>();auto&g=terrain->geo;g.width=g.height=4;g.offsetX=g.offsetY=4;g.rw=g.rh=g.rawWidth=g.rawHeight=8;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=4096;g.bounds={4,4,0,0,4,4};assert(g.valid());
 terrain->reveal=true;terrain->colors.resize(512*512);terrain->baseColors.resize(512*512);terrain->art.resize(512*512);terrain->colors[5*512+5]=terrain->baseColors[5*512+5]=0x193347;terrain->art[5*512+5].type=0x250000;
 radar::Frame frame;frame.terrain=terrain;frame.frame=1;frame.tick=1;frame.markerScanValid=true;
 put(0xA8B230,uint32_t(0x600000));put(0x60352C,100);put(0x603534,std::array<int,3>{{100,100,100}});
 auto render=[&]{return renderer.render(frame,600,300,read,assets,{},1,false,true,false,true);};
 auto first=render();assert(first&&first->pixels.size()==180000);revisionReads=0;double start=now();unsigned reused=0;
 for(int n=0;n<120;++n){auto next=render();assert(next&&next->pixels==first->pixels);reused+=renderer.stageStats().reused;
#ifndef BEFORE_RENDERER
  assert(next==first&&renderer.stageStats().reused&&!xbrzadapter::gpuUsed());
#endif
 }
 const double mean=(now()-start)/120;assert(revisionReads==120);
 // No game frame/tick change: a live ambient or raw palette change still
 // rebuilds and paints before output can be reused.
 put(0x60352C,50);auto dark=render();assert(dark&&dark!=first&&!renderer.stageStats().reused&&dark->pixels!=first->pixels);
 assets.color=0xff44aa88;auto recolored=render();assert(recolored&&recolored!=dark&&!renderer.stageStats().reused&&recolored->pixels!=dark->pixels);
 failAt=0x822CF8;assert(!render());failAt=0;auto recovered=render();assert(recovered&&recovered->pixels==recolored->pixels);
 // CPU radar decorations remain on the conservative fresh-painter route;
 // there is no equivalence claim between CPU doubles and GPU float words.
 auto extras=std::make_shared<sceneextras::Snapshot>();extras->radar.push_back({0,2,2,1,0,1});frame.extras=extras;
 auto decorated=render();assert(decorated&&!renderer.stageStats().reused&&decorated->pixels!=recolored->pixels);auto decorated2=render();assert(decorated2&&!renderer.stageStats().reused&&decorated2!=decorated&&decorated2->pixels==decorated->pixels);
 extras=std::make_shared<sceneextras::Snapshot>(*extras);extras->radar[0].fade=0;frame.extras=extras;auto faded=render();assert(faded&&faded->pixels!=decorated->pixels&&!renderer.stageStats().reused);
 frame.extras.reset();assert(render());auto stable=render();assert(stable);
#ifndef BEFORE_RENDERER
 assert(renderer.stageStats().reused);
#endif
 // LightConvert full-color rows can change without ambient/intensity/tint.
 // The sampled immutable table must invalidate the retained ground too.
 terrain=std::make_shared<radar::Terrain>(*terrain);terrain->art[5*512+5].cell=0x700000;frame.terrain=terrain;assets.indexed=true;++assets.color;
 put(0x700104,uint32_t(65536));put(0x700034,uint32_t(0x710000));put(0x70010A,std::array<int16_t,3>{{1000,1000,1000}});
 put(0x710000,uint32_t(0x7E5358));put(0x710004,2);put(0x71016C,1);put(0x710170,uint32_t(0x720000));put(0x710198,std::array<int,3>{{1000,1000,1000}});put(0x720002,uint16_t(0xf800));put(0xA8ED84,1);
 auto red=render();assert(red);put(0x720002,uint16_t(0x07e0));put(0xA8ED84,2);++frame.frame;
 auto green=render();assert(green);const bool tableChanged=green->pixels!=red->pixels;
#ifndef BEFORE_RENDERER
 assert(tableChanged&&!renderer.stageStats().reused);
 auto unchanged=render();assert(unchanged==green&&renderer.stageStats().reused);
 put(0xA8ED84,3);++frame.frame;auto sameTable=render();assert(sameTable==green&&renderer.stageStats().reused); // same bytes keep immutable owner
#endif
 std::printf("PASS actual SceneRenderer CPU: every-request revision/light/palette; immutable reuse; same-frame light/palette invalidation; failed read recovery; CPU decoration exclusion. repeated_mean_ms=%.6f reused=%u/120 converter_table_changed=%u\n",mean,reused,unsigned(tableChanged));
}
