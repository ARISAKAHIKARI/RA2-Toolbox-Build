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
template<class T>void put(uint32_t p,const T&v){std::memcpy(memory.data()+p,&v,sizeof v);}
struct Read {
 bool operator()(uint32_t at,void*out,size_t n)const{
  if(at>=Objects&&n==0x148)++cellReads;
  if(uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;
  std::memcpy(out,memory.data()+at,n);return true;
 }
};
struct Result {std::shared_ptr<const radar::Terrain>terrain;unsigned steps=0;double milliseconds=0;};
Result sample(bool compact){radar::Sampler<Read>sampler(Read{});Result result;auto started=std::chrono::steady_clock::now();
 while(!sampler.hasTerrain()){cellReads=0;assert(sampler.step(0,4096,true,compact));maximumCellReads=std::max(maximumCellReads,cellReads);assert(cellReads<=4096);++result.steps;assert(result.steps<=64);}
 result.terrain=sampler.terrainFrame(1).terrain;result.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return result;
}
}
int main(){
 constexpr int width=128,height=128;mapbounds::Bounds bounds{width,height,0,0,width,height};
 put(radar::Map+0xF4,width);put(radar::Map+0xF8,height);put(radar::Map+0x104,width);put(radar::Map+0x108,height);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,512*512);put(radar::Map+0x1488,1.f);put(radar::Map+0x1490,width);put(radar::Map+0x1498,width);
 put(radar::Map+0x14A4,2*width);put(radar::Map+0x14A8,2*height);put(radar::Map+0x1240,2*width);put(radar::Map+0x1244,2*height);
 put(0xA83D4C,House);put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 size_t count=0;for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(bounds.legal(x,y)){
  uint32_t p=Objects+uint32_t(count++)*0x150;put(Cells+uint32_t(y*512+x)*4,p);put(p+0x24,int16_t(x));put(p+0x26,int16_t(y));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(1));put(p+0xEC,(x+y)%12);put(p+0x38,-1);put(p+0x44,-1);put(p+0x48,-1);
 }
 auto before=sample(false),after=sample(true);assert(before.steps==64&&after.steps<before.steps);
 assert(before.terrain->colors==after.terrain->colors&&before.terrain->baseColors==after.terrain->baseColors&&before.terrain->shade==after.terrain->shade);
 assert(before.terrain->art.size()==after.terrain->art.size());for(size_t n=0;n<before.terrain->art.size();++n){const auto&a=before.terrain->art[n],&b=after.terrain->art[n];assert(a.cell==b.cell&&a.type==b.type&&a.sub==b.sub&&a.level==b.level&&a.sourceHidden==b.sourceHidden&&a.sourceColor==b.sourceColor&&a.land==b.land);}
 radar::Sampler<Read>sampler(Read{});assert(sampler.step(0,4096,true,true)&&!sampler.hasTerrain());failAt=Objects+5000*0x150;assert(!sampler.step(0,4096,true,true)&&!sampler.hasTerrain());failAt=0;for(int n=0;n<16&&!sampler.hasTerrain();++n)assert(sampler.step(0,4096,true,true));assert(sampler.hasTerrain());
 std::printf("PASS actual Sampler 128x128 (%zu physical Cells): complete publication %u -> %u bounded steps, unchanged colors/shade/material identities; maximum %zu physical Cell reads/step <=4096; short read rejects/retries complete bank. CPU complete sweep %.6f -> %.6f ms (not frame rate).\n",count,before.steps,after.steps,maximumCellReads,before.milliseconds,after.milliseconds);
}
