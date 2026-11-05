#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/radar_map.h"
#include <cassert>
#include <chrono>
#include <cstdio>
using namespace external;
namespace {
constexpr uint32_t Cells=0x100000,Objects=0xC00000,House=0x300000;
std::vector<uint8_t>memory(32*1024*1024);size_t cellReads=0,maximumCellReads=0;uint32_t failAt=0;
template<class T>void put(uint32_t p,const T&v){assert(uint64_t(p)+sizeof v<=memory.size());std::memcpy(memory.data()+p,&v,sizeof v);}
struct Read {
 bool operator()(uint32_t at,void*out,size_t n)const{
  if(at>=Objects&&n==0x148)++cellReads;
  if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;
  std::memcpy(out,memory.data()+at,n);return true;
 }
};
size_t populate(int width,int height){
 std::fill(memory.begin(),memory.end(),0);failAt=0;mapbounds::Bounds bounds{width,height,0,0,width,height};
 put(radar::Map+0xF4,width);put(radar::Map+0xF8,height);put(radar::Map+0x104,width);put(radar::Map+0x108,height);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,512*512);put(radar::Map+0x1488,1.f);put(radar::Map+0x1490,width);put(radar::Map+0x1498,width);
 put(radar::Map+0x14A4,2*width);put(radar::Map+0x14A8,2*height);put(radar::Map+0x1240,2*width);put(radar::Map+0x1244,2*height);
 put(0xA83D4C,House);put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 size_t count=0;for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(bounds.legal(x,y)){
  uint32_t p=Objects+uint32_t(count++)*0x150;put(Cells+uint32_t(y*512+x)*4,p);put(p+0x24,int16_t(x));put(p+0x26,int16_t(y));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(1));put(p+0xEC,(x+y)%12);put(p+0x38,-1);put(p+0x44,-1);put(p+0x48,-1);
 }
 return count;
}
void equal(const radar::Terrain&a,const radar::Terrain&b){
 assert(a.geo.same(b.geo)&&a.geo.projectionAdjusted==b.geo.projectionAdjusted&&a.geo.backingWidth==b.geo.backingWidth&&a.geo.backingHeight==b.geo.backingHeight&&a.geo.backingOffsetX==b.geo.backingOffsetX&&a.geo.backingOffsetY==b.geo.backingOffsetY&&a.reveal==b.reveal);
 assert(a.colors==b.colors&&a.baseColors==b.baseColors&&a.shade==b.shade&&a.art.size()==b.art.size());
 for(size_t n=0;n<a.art.size();++n){const auto&x=a.art[n],&y=b.art[n];
  assert(x.cell==y.cell&&x.type==y.type&&x.overlayType==y.overlayType&&x.overlaySourceType==y.overlaySourceType&&x.smudgeType==y.smudgeType&&x.rubbleType==y.rubbleType&&x.flags==y.flags&&x.pixelFX==y.pixelFX&&x.land==y.land&&x.smudgeData==y.smudgeData&&x.overlay==y.overlay&&x.sub==y.sub&&x.level==y.level&&x.overlayFrame==y.overlayFrame&&x.sourceHidden==y.sourceHidden&&x.sourceColor==y.sourceColor);
 }
}
struct Result {std::shared_ptr<const radar::Terrain>terrain;unsigned steps=0;double milliseconds=0;};
Result sample(bool compact,int budget,uint32_t reveal=0){radar::Sampler<Read>sampler(Read{});Result result;auto started=std::chrono::steady_clock::now();
 while(!sampler.hasTerrain()){cellReads=0;assert(sampler.step(reveal,budget,true,compact));maximumCellReads=std::max(maximumCellReads,cellReads);assert(cellReads<=size_t(budget));++result.steps;assert(result.steps<=64);}
 result.terrain=sampler.terrainFrame(1).terrain;result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return result;
}
}
int main(){
 for(int width:{64,128})for(int budget:{4096,8192,16384})for(uint32_t reveal:{0u,1u}){
  const size_t count=populate(width,width);auto before=sample(false,budget,reveal),after=sample(true,budget,reveal);equal(*before.terrain,*after.terrain);
  assert(before.steps==unsigned((512*512+budget-1)/budget)&&after.steps<before.steps);
  std::printf("PASS actual Sampler %dx%d (%zu physical Cells), budget=%d reveal=%u: complete bank %u -> %u bounded steps; all Terrain fields equal; CPU sweep %.6f -> %.6f ms (not game FPS).\n",width,width,count,budget,reveal,before.steps,after.steps,before.milliseconds,after.milliseconds);
 }
 populate(128,128);radar::Sampler<Read>sampler(Read{});assert(sampler.step(0,4096,true,true)&&!sampler.hasTerrain());failAt=Objects+5000*0x150;assert(!sampler.step(0,4096,true,true)&&!sampler.hasTerrain());failAt=0;
 for(int n=0;n<16&&!sampler.hasTerrain();++n)assert(sampler.step(0,4096,true,true));assert(sampler.hasTerrain());equal(*sampler.terrainFrame(1).terrain,*sample(false,4096).terrain);
 // A partial scan cannot survive a map-geometry replacement. Check with both
 // host budgets: the new complete bank must equal a fresh scalar reference.
 for(int budget:{8192,16384}){
  populate(64,64);radar::Sampler<Read>changed(Read{});assert(changed.step(0,budget,true,true)&&changed.hasTerrain());
  populate(128,128);assert(changed.step(0,budget,true,true)&&!changed.hasTerrain());
  for(int n=0;n<32&&!changed.hasTerrain();++n)assert(changed.step(0,budget,true,true));assert(changed.hasTerrain());equal(*changed.terrainFrame(1).terrain,*sample(false,budget).terrain);
  // Replace geometry again while the previous map's next sweep is partial.
  assert(changed.step(0,budget,true,true));populate(64,64);assert(changed.step(0,budget,true,true)&&changed.hasTerrain());equal(*changed.terrainFrame(1).terrain,*sample(false,budget).terrain);
 }
 std::printf("PASS failed-read rejection/recovery and in-progress map-geometry replacement at both host budgets; maximum %zu physical Cell reads/step. Cadence delay is distinct from measured CPU sweep cost.\n",maximumCellReads);
}
