#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include RA2_REFERENCE_LIVE
#include <cassert>
#include <cstdio>
#include <chrono>
using namespace external;
namespace {
constexpr uint32_t Cells=0x100000,Objects=0xC00000,House=0x300000,Scenario=0x600000;
std::vector<uint8_t>memory(64*1024*1024);size_t freshCalls=0;uint32_t failAt=0;
template<class T>void put(uint32_t at,const T&v){std::memcpy(memory.data()+at,&v,sizeof v);}
struct Read {bool operator()(uint32_t at,void*out,size_t n)const{if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;}
 bool fresh(uint32_t at,void*out,size_t n)const{++freshCalls;return (*this)(at,out,n);}bool readFresh(uint32_t at,void*out,size_t n){return fresh(at,out,n);}bool imagePath(wchar_t*,DWORD*){return false;}};
using Base=cameraassets::Context<Read>;
struct Assets:Base {using Base::Base;
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xff2266aa);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,0xff2266aau);out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
 bool shroud(bool,int f,voxel::Image&out){out.canvasWidth=out.width=60;out.canvasHeight=out.height=30;out.x=-30;out.y=-15;out.indices.assign(1800,254);for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs((x+.5)-30)/30.+std::abs((y+.5)-15)/15.<=1)out.indices[y*60+x]=f==15?0:127;return true;}
};
double clockMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void capturePerformance(const radar::Geometry&g,bool lifted){
 Read read;radar::shroudmask::BeforeLive original;radar::shroudmask::BeforeLiftedLive before;radar::shroudmask::Live after;radar::shroudmask::Snapshot a,b;
 if(lifted)assert(!original.capture(read,g,0,0,g.rawWidth*30.,g.rawHeight*15.,a));
 else assert(original.capture(read,g,0,0,g.rawWidth*30.,g.rawHeight*15.,a));
 auto oldCapture=[&]{return lifted?before.capture(read,g,0,0,g.rawWidth*30.,g.rawHeight*15.,a):original.capture(read,g,0,0,g.rawWidth*30.,g.rawHeight*15.,a);};
 assert(oldCapture()&&after.capture(read,g,0,0,g.rawWidth*30.,g.rawHeight*15.,b)&&a.sameMask(b));double oldMs=0,newMs=0;
 for(int n=0;n<8;++n){auto oldRun=[&]{auto start=clockMs();assert(oldCapture());oldMs+=clockMs()-start;};auto newRun=[&]{auto start=clockMs();assert(after.capture(read,g,0,0,g.rawWidth*30.,g.rawHeight*15.,b));newMs+=clockMs()-start;};if(n&1){oldRun();newRun();}else{newRun();oldRun();}assert(a.sameMask(b));}
 std::printf("PASS visibility warm %dx%d: %zu captured cells, mean %.6f -> %.6f ms (8 paired captures); %s; unordered page bytes refresh every epoch, <=16384 retained pages (64MiB page payload maximum per Live instance; not process resident RAM).\n",g.width,g.height,b.cells.size(),oldMs/8,newMs/8,lifted?"large comparison lifts ONLY original 65536-cell admission to reach its unchanged 8192-page/map algorithm; original admission separately rejected":"exact original 2bb Live baseline");
}
}
int main(){
 auto terrain=std::make_shared<radar::Terrain>();auto&g=terrain->geo;g.width=g.height=256;g.offsetX=g.offsetY=256;g.rw=g.rh=g.rawWidth=g.rawHeight=512;g.factor=1;g.cells=Cells;g.house=House;g.count=512*512;g.bounds={256,256,0,0,256,256};assert(g.valid());
 put(radar::Map+0xF4,g.width);put(radar::Map+0xF8,g.height);put(radar::Map+0x104,g.width);put(radar::Map+0x108,g.height);put(radar::Map+0x13C,Cells);put(radar::Map+0x140,g.count);put(radar::Map+0x1490,g.offsetX);put(radar::Map+0x1498,g.offsetY);put(radar::Map+0x1240,g.rawWidth);put(radar::Map+0x1244,g.rawHeight);
 put(0xA83D4C,House);put(0xA8B230,Scenario);put(Scenario+0x352C,100);put(Scenario+0x3534,std::array<int,3>{{100,100,100}});put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 terrain->colors.resize(512*512);terrain->baseColors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);size_t physical=0;
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(g.legal(x,y)){
  const size_t n=size_t(y)*512+x;const auto p=Objects+uint32_t(physical++)*0x150;put(Cells+uint32_t(n)*4,p);put(p+0x24,int16_t(x));put(p+0x26,int16_t(y));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(1));
  terrain->colors[n]=terrain->baseColors[n]=0x193347;terrain->shade[n]={-1,-1};auto&c=terrain->art[n];c.cell=p;c.type=0x250000;c.sourceColor=0x193347;
 }
 assert(physical>65536);Read read;Assets assets(read);radar::shroudmask::Live visibility;radar::shroudmask::Snapshot snapshot;auto started=std::chrono::steady_clock::now();assert(visibility.capture(read,g,0,0,15360,7680,snapshot)&&snapshot.cells.size()>65536);const auto visibilityMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 gpushroud::Builder builder;std::shared_ptr<const gpushroud::Plan>mask;assert(builder.build(g,snapshot.cells,320,160,15360,7680,false,[&](bool fog,int f,voxel::Image&out){return assets.shroud(fog,f,out);},mask));gpushroud::Packed packed;assert(gpushroud::pack(*mask,packed)&&packed.metadata.size()<=gpushroud::MaximumMetadataWords);assert(!gpushroud::withinGpuBudget(*mask));
 radar::Frame frame;frame.terrain=terrain;frame.frame=1;frame.tick=1;frame.markerScanValid=true;auto extras=std::make_shared<sceneextras::Snapshot>();extras->frozenValid=true;frame.extras=extras;
 radar::SceneRenderer<Assets>renderer;renderer.outputSized(true);xbrzadapter::gpuPresentation(false);std::shared_ptr<const radar::Scene>scene;for(int pass=0;pass<40&&!scene;++pass)scene=renderer.render(frame,320,160,read,assets,{},1,false,true,false,true);assert(scene&&scene->pixels.size()==320*160&&renderer.nativeStats().cells>65536);auto pixels=scene->pixels;
 frame.tick+=34;failAt=Objects+0x12C;assert(!renderer.render(frame,320,160,read,assets,{},1,false,true,false,true));assert(scene->pixels==pixels);failAt=0;auto recovered=renderer.render(frame,320,160,read,assets,{},1,false,true,false,true);assert(recovered&&recovered->pixels==pixels);
 assets.presentationGPU(true);frame.tick+=34;xbrzadapter::gpuPresentation(true);auto fallback=renderer.render(frame,320,160,read,assets,{},1,false,true,false,true);assert(fallback&&fallback->pixels==pixels&&!fallback->gpu);xbrzadapter::gpuPresentation(false);
 // Packed reference offsets are unsigned 32-bit words. Keep the original
 // 64MiB mask metadata and 8M coordinate/reference ceilings after expansion.
 assert(packed.params.referenceOffset<=packed.metadata.size());assert(mask->coordinates.size()<=gpushroud::MaximumCoordinates&&mask->references.size()<=gpushroud::MaximumReferences);
 std::printf("PASS actual 256x256 map: %zu physical Cells, %zu visibility cells, %zu terrain draws at 320x160 (15360x7680 source), page count=%zu, capture=%.6f ms, fresh calls=%zu; shroud stamps=%zu metadata=%zu bytes (<64MiB); complete CPU render and short-read recovery equal. DrawJob=%zu bytes; bounded 262144-record DrawJob storage=%zu bytes. GPU 16-bit index admission remains 65536, larger scenes use complete CPU replay.\n",physical,snapshot.cells.size(),size_t(renderer.nativeStats().cells),visibility.stats().pages,visibilityMs,freshCalls,mask->stamps.size(),packed.metadata.size()*4,sizeof(vulkancompute::DrawJob),vulkancompute::MaximumSceneDraws*sizeof(vulkancompute::DrawJob));
 capturePerformance(g,true);
 auto small=g;small.width=small.height=small.offsetX=small.offsetY=128;small.rw=small.rh=small.rawWidth=small.rawHeight=256;small.bounds={128,128,0,0,128,128};
 put(radar::Map+0xF4,128);put(radar::Map+0xF8,128);put(radar::Map+0x104,128);put(radar::Map+0x108,128);put(radar::Map+0x1490,128);put(radar::Map+0x1498,128);put(radar::Map+0x1240,256);put(radar::Map+0x1244,256);std::fill(memory.begin()+Cells,memory.begin()+Cells+512*512*4,0);size_t smallCount=0;
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(small.legal(x,y)){auto p=Objects+uint32_t(smallCount++)*0x150;put(Cells+uint32_t(y*512+x)*4,p);put(p+0x24,int16_t(x));put(p+0x26,int16_t(y));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(1));}assert(smallCount==32640);capturePerformance(small,false);
}
