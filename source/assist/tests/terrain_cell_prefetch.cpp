#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/shared_reader.h"
#include RA2_REFERENCE_SCENE
#include <cassert>
#include <cstdio>
using namespace external;
namespace {
constexpr uint32_t Globals=0x10000,GlobalBytes=0xBF0000,CellCount=10000,CellStride=0x150,CellBytes=CellCount*CellStride+4096;
uint32_t globalStorage=0,cellsStorage=0;
uint32_t storage(uint32_t at){return at>=Globals&&at<Globals+GlobalBytes?globalStorage+at-Globals:at;}
template<class T>void put(uint32_t at,const T&v){std::memcpy(reinterpret_cast<void*>(uintptr_t(storage(at))),&v,sizeof v);}
void*arena(){constexpr SIZE_T bytes=SIZE_T(GlobalBytes)+2*CellBytes;for(uintptr_t at=0x10000000;at+bytes<0x80000000ull;){MEMORY_BASIC_INFORMATION r{};if(!VirtualQuery(reinterpret_cast<void*>(at),&r,sizeof r))break;const uintptr_t end=uintptr_t(r.BaseAddress)+r.RegionSize;if(r.State==MEM_FREE){const uintptr_t first=(at+65535)&~uintptr_t(65535);if(first+bytes<=end&&first+bytes<0x80000000ull)if(auto p=VirtualAlloc(reinterpret_cast<void*>(first),bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))return p;}if(end<=at)break;at=end;}return nullptr;}
struct Read {
 ProcessReader*reader;unsigned*prefetchCalls;size_t*lastCount;unsigned*batchMisses;std::vector<uint32_t>*firstBatch;
 bool operator()(uint32_t at,void*out,size_t n)const{return reader->read(storage(at),out,n);}
 bool readFresh(uint32_t at,void*out,size_t n){return reader->readFresh(storage(at),out,n);}
 bool fresh(uint32_t at,void*out,size_t n)const{return reader->readFresh(storage(at),out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
 void prefetch(const std::vector<uint32_t>&addresses,size_t span)const{assert(span==4096);++*prefetchCalls;*lastCount=addresses.size();if(firstBatch->empty())*firstBatch=addresses;const auto misses=reader->prefetchMisses;reader->prefetch(addresses,span);*batchMisses+=reader->prefetchMisses-misses;}
};
using Base=cameraassets::Context<Read>;
struct Assets:Base {using Base::Base;
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xff2266aa);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&pal,voxel::Image&out){out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,pal[1]);out.indices.assign(1800,1);out.nativeDepth.enabled=true;out.nativeDepth.gradient=nativesurface::Gradient::Flat;return true;}
};
double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
}
int main(){
 auto memory=arena();assert(memory);globalStorage=uint32_t(uintptr_t(memory));cellsStorage=globalStorage+GlobalBytes;
 put(0xA8B230,uint32_t(0x600000));put(0x60352C,100);put(0x603534,std::array<int,3>{{100,100,100}});put(0x710198,std::array<int,3>{{1000,1000,1000}});put(0xA8ED84,1);
 auto terrain=std::make_shared<radar::Terrain>();auto&g=terrain->geo;g.width=g.height=100;g.offsetX=g.offsetY=100;g.rw=g.rh=g.rawWidth=g.rawHeight=200;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=512*512;g.bounds={100,100,0,0,100,100};assert(g.valid());
 terrain->reveal=true;terrain->colors.resize(512*512);terrain->baseColors.resize(512*512);terrain->art.resize(512*512);std::vector<size_t>indices;
 for(int y=1;y<512&&indices.size()<CellCount;++y)for(int x=1;x<512&&indices.size()<CellCount;++x){mapbounds::TileOrigin at;if(!g.tileOrigin(x,y,0,at))continue;const size_t i=y*512+x;auto&c=terrain->art[i];c.cell=cellsStorage+uint32_t((indices.size()*7919)%CellCount)*CellStride;c.type=0x250000;terrain->colors[i]=terrain->baseColors[i]=0x193347;put(c.cell+0x104,uint32_t(65536));put(c.cell+0x34,uint32_t(0x710000));put(c.cell+0x10A,std::array<int16_t,3>{{1000,1000,1000}});indices.push_back(i);}
 assert(indices.size()==CellCount);radar::Frame frame;frame.terrain=terrain;frame.frame=1;frame.tick=1;frame.markerScanValid=true;
 auto ordered=indices;std::stable_sort(ordered.begin(),ordered.end(),[](size_t a,size_t b){return a%512+a/512<b%512+b/512;});std::vector<uint32_t>expectedFirst;unsigned crossed=0;for(size_t n=0;n<ordered.size();++n){const auto p=terrain->art[ordered[n]].cell;const auto a=p&~4095u,b=(p+0x10F)&~4095u;if(a!=b)++crossed;if(n<4096){expectedFirst.push_back(a);if(a!=b)expectedFirst.push_back(b);}}std::sort(expectedFirst.begin(),expectedFirst.end());expectedFirst.erase(std::unique(expectedFirst.begin(),expectedFirst.end()),expectedFirst.end());assert(crossed>0);
 ProcessReader oldReader,newReader;assert(oldReader.open(GetCurrentProcessId())&&newReader.open(GetCurrentProcessId()));unsigned oldBatches=0,newBatches=0,oldMisses=0,newMisses=0;size_t oldCount=0,newCount=0;std::vector<uint32_t>oldBatch,newBatch;Read oldRead{&oldReader,&oldBatches,&oldCount,&oldMisses,&oldBatch},read{&newReader,&newBatches,&newCount,&newMisses,&newBatch};Assets oldAssets(oldRead),assets(read);
 radar::BeforeSceneRenderer<Assets>before;radar::SceneRenderer<Assets>after;before.outputSized(true);after.outputSized(true);xbrzadapter::gpuPresentation(false);
 auto oldRender=[&]{oldReader.begin();return before.render(frame,600,300,oldRead,oldAssets,{},1,false,true,false,true);};
 auto newRender=[&]{newReader.begin();return after.render(frame,600,300,read,assets,{},1,false,true,false,true);};
 std::shared_ptr<const radar::Scene>oldImage,newImage;
 auto settle=[&]{for(int n=0;n<16;++n){oldImage=oldRender();newImage=newRender();assert(bool(oldImage)==bool(newImage));if(oldImage){assert(oldImage->pixels==newImage->pixels);if(before.materialsReady()&&after.materialsReady())return;}}assert(false);};
 oldImage=oldRender();newImage=newRender();assert(newBatches==1&&newBatch==expectedFirst);assert(bool(oldImage)==bool(newImage));settle();assert(oldBatches==0);
 uint64_t oldCalls=0,newCalls=0,newPages=0;double oldMs=0,newMs=0,oldTerrain=0,newTerrain=0;
 for(int n=0;n<24;++n){++frame.frame;++frame.tick;put(0xA8ED84,frame.frame);auto a=[&]{const auto start=now();oldImage=oldRender();oldMs+=now()-start;oldCalls+=oldReader.reads;oldTerrain+=before.stageStats().terrainMs;};auto b=[&]{const auto start=now();newImage=newRender();newMs+=now()-start;newCalls+=newReader.reads;newPages+=newReader.prefetchedPages;newTerrain+=after.stageStats().terrainMs;};if(n&1){a();b();}else{b();a();}assert(oldImage&&newImage&&oldImage->pixels==newImage->pixels);assert(after.stageStats().terrainLightCells==CellCount&&newCount<CellCount);}
 assert(newCalls*4<oldCalls&&newPages>0);
 std::printf("PASS actual SceneRenderer + self ProcessReader: %u packed Cells x 24 fresh observations, RPM %llu -> %llu/observation; total mean %.6f -> %.6f ms; terrain mean %.6f -> %.6f ms; full output identical, every Cell revalidated.\n",CellCount,(unsigned long long)(oldCalls/24),(unsigned long long)(newCalls/24),oldMs/24,newMs/24,oldTerrain/24,newTerrain/24);
 // Keep the engine frame unchanged while changing every Cell's evaluated
 // intensity, then shared converter tint: the next read epoch must see both.
 auto original=newImage->pixels;for(auto index:indices)put(terrain->art[index].cell+0x10A,std::array<int16_t,3>{{500,500,500}});settle();assert(newImage->pixels!=original);auto dimmed=newImage->pixels;
 put(0x710198,std::array<int,3>{{500,1000,1500}});settle();assert(newImage->pixels!=dimmed);
 // Failed speculative span still uses the original per-field fallback. Do
 // not claim a stronger failure contract than lighting::cell actually has.
 const auto protectedPage=cellsStorage&~4095u;DWORD previous=0,ignored=0;const auto priorMisses=newMisses;assert(VirtualProtect(reinterpret_cast<void*>(uintptr_t(protectedPage)),4096,PAGE_NOACCESS,&previous));settle();assert(newMisses>priorMisses);assert(VirtualProtect(reinterpret_cast<void*>(uintptr_t(protectedPage)),4096,previous,&ignored));settle();
 auto retained=newImage;const auto theatrePage=storage(0x822CF8)&~4095u;assert(VirtualProtect(reinterpret_cast<void*>(uintptr_t(theatrePage)),4096,PAGE_NOACCESS,&previous));assert(!oldRender()&&!newRender());assert(retained->pixels==newImage->pixels);assert(VirtualProtect(reinterpret_cast<void*>(uintptr_t(theatrePage)),4096,previous,&ignored));settle();assert(newImage->pixels==retained->pixels);
 // The map keeps the same geometry and pixels, but every physical Cell moves.
 // A new source must rebuild the address plan even when its size is unchanged.
 std::memcpy(reinterpret_cast<void*>(uintptr_t(cellsStorage+CellBytes)),reinterpret_cast<void*>(uintptr_t(cellsStorage)),CellCount*CellStride);auto moved=std::make_shared<radar::Terrain>(*terrain);for(auto index:indices)moved->art[index].cell+=CellBytes;frame.terrain=moved;newBatch.clear();settle();assert(newImage->pixels==retained->pixels&&!newBatch.empty());for(auto page:newBatch)assert(page>=((cellsStorage+CellBytes)&~4095u));
 // A local camera never performs this whole-map prefetch. Existing typed
 // read/fresh behavior and the plain-callable fallback both remain usable.
 radar::SceneRenderer<Assets>local;local.outputSized(true);unsigned batches=newBatches;bool localReady=false;for(int n=0;n<10;++n){newReader.begin();if(local.render(frame,320,240,read,assets,{1000,500,640,480,true},1,false,true,false,true)){localReady=true;break;}}assert(localReady&&newBatches==batches);
 auto callable=[&](uint32_t at,void*out,size_t n){return read(at,out,n);};radar::SceneRenderer<Assets>plain;plain.outputSized(true);bool plainReady=false;for(int n=0;n<16;++n){newReader.begin();if(auto result=plain.render(frame,600,300,callable,assets,{},1,false,true,false,true)){assert(result->pixels==newImage->pixels);plainReady=true;break;}}assert(plainReady&&newBatches==batches);
 std::puts("PASS initial build exact 4096-Cell chunk, randomized Cell address order, cross-page span coverage, moved source invalidates page plan, no local-camera prefetch, plain callable fallback, same-frame intensity/tint output changes, speculative unreadable page parity, failed required revision retains complete owner and recovery.");
 assert(VirtualFree(memory,0,MEM_RELEASE));
}
