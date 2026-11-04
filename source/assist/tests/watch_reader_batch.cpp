#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/radar_map.h"
#include "../../external_reader/source/shared_reader.h"
#include "../source/watch_read_view.h"
#include <functional>
#include <tuple>
#include <cassert>
#include <cstdio>
#include <chrono>
using namespace external;
namespace {
constexpr uint32_t Globals=0x700000,GlobalBytes=0x500000,CellBytes=512*512*4,ObjectBytes=48*1024*1024,Type=0xB20000,House=0xB40000;
static uint32_t globalStorage=0,Cells=0,Objects=0;
static uint32_t storage(uint32_t address){return address>=Globals&&address<Globals+GlobalBytes?globalStorage+(address-Globals):address;}
template<class T>void put(uint32_t address,T value){std::memcpy((void*)(uintptr_t)storage(address),&value,sizeof value);}
static void* reserveFixture(){
 // ASLR, the CRT heap and loaded modules can occupy 0x700000..0xBFFFFF.
 // Inspect free regions rather than overwriting those mappings or depending
 // on a lucky launch. All fixture pages still reside below the reader's 2 GiB
 // limit; only the game's fixed globals are translated, with page offsets kept.
 constexpr SIZE_T bytes=SIZE_T(GlobalBytes)+CellBytes+ObjectBytes;
 for(uintptr_t at=0x10000000;at+bytes<=0x80000000ull;){
  MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery((void*)at,&region,sizeof region))break;
  const uintptr_t end=uintptr_t(region.BaseAddress)+region.RegionSize;
  if(region.State==MEM_FREE){const uintptr_t start=(at+65535)&~uintptr_t(65535);
   if(start+bytes<=end&&start+bytes<=0x80000000ull)
    if(void*p=VirtualAlloc((void*)start,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))return p;
  }
  if(end<=at)break;at=end;
 }
 return nullptr;
}
struct Read {
 ProcessReader*reader;bool batch;
 bool operator()(uint32_t address,void*out,size_t n)const{return reader->read(storage(address),out,n);}
 bool read(uint32_t address,void*out,size_t n)const{return (*this)(address,out,n);}
 void prefetch(const std::vector<uint32_t>&addresses,size_t span)const{if(batch)reader->prefetch(addresses,span);}
};
struct Result {std::shared_ptr<const radar::Terrain> terrain;uint64_t calls=0;double ms=0;};
template<class Adapter>Result sampleWith(Adapter adapter,ProcessReader&reader,unsigned passes){
 radar::Sampler<Adapter> sampler(adapter);Result result;
 const auto started=std::chrono::steady_clock::now();
 for(unsigned pass=0;pass<passes;++pass){
  // Ordinary ongoing terrain refresh, including a changed material/visibility
  // field between complete sweeps. Both variants see the same input sequence.
  put(0xA8ED84,int(pass+1));put(Objects+0xEC,int(pass%12));put(Objects+0x12C,uint8_t(pass&1?0x10:0));
  const unsigned previous=sampler.revision();
  do{reader.begin();assert(sampler.step(0,8192,true));result.calls+=reader.reads;}while(!sampler.hasTerrain()||sampler.revision()==previous);
  result.terrain=sampler.terrainFrame(pass+1).terrain;assert(result.terrain);
 }
 result.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return result;
}
Result sample(bool typed,unsigned passes){
 ProcessReader reader;assert(reader.open(GetCurrentProcessId()));Read backend{&reader,true};
 if(typed)return sampleWith(watchscene::ReadView<Read>{&backend},reader,passes);
 return sampleWith(std::function<bool(uint32_t,void*,size_t)>([&](uint32_t p,void*out,size_t n){return backend.read(p,out,n);}),reader,passes);
}
}
int main(){
 MEMORY_BASIC_INFORMATION fixedRegion{};assert(VirtualQuery((void*)(uintptr_t)Globals,&fixedRegion,sizeof fixedRegion));
 void*arena=reserveFixture();assert(arena);globalStorage=uint32_t(uintptr_t(arena));Cells=globalStorage+GlobalBytes;Objects=Cells+CellBytes;
 std::printf("Fixture address layout: module=%p heap=%p old_global_region=%p state=%08lX type=%08lX size=%llu arena=%p\n",GetModuleHandleW(nullptr),GetProcessHeap(),fixedRegion.BaseAddress,fixedRegion.State,fixedRegion.Type,(unsigned long long)fixedRegion.RegionSize,arena);
 constexpr int Width=128,Height=128;mapbounds::Bounds bounds{Width,Height,0,0,Width,Height};
 put(radar::Map+0xF4,Width);put(radar::Map+0xF8,Height);put(radar::Map+0x104,Width);put(radar::Map+0x108,Height);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,512*512);put(radar::Map+0x1488,1.f);
 put(radar::Map+0x1490,Width);put(radar::Map+0x1498,Width);put(radar::Map+0x14A4,Width*2);put(radar::Map+0x14A8,Height*2);
 put(radar::Map+0x1240,Width*2);put(radar::Map+0x1244,Height*2);put(0xA83D4C,House);
 put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 for(auto address:{0xA8ED28u,0xA83D80u,0xA8EC18u}){put(address+4,uint32_t(0xB10000));put(address+8,1);put(address+16,1);}put(0xB10000,Type);put(Type+0x2F0,1);
 unsigned count=0;size_t firstIndex=0;
 for(int y=0;y<512;++y)for(int x=0;x<512;++x)if(bounds.legal(x,y)){
  const size_t index=size_t(y)*512+x;if(!count)firstIndex=index;const uint32_t p=Objects+count++*0x150;
  put(Cells+uint32_t(index)*4,p);put(p+0x24,int16_t(x));put(p+0x26,int16_t(y));put(p+0xEC,int((x+y)%12));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(1));put(p+0x38,0);put(p+0x44,-1);put(p+0x48,-1);
 }
 // Warm both paths before measuring, then compare all semantic output fields.
 sample(false,1);sample(true,1);auto scalar=sample(false,8),batched=sample(true,8);
 assert(scalar.terrain->colors==batched.terrain->colors&&scalar.terrain->baseColors==batched.terrain->baseColors&&scalar.terrain->shade==batched.terrain->shade);
 assert(scalar.terrain->art.size()==batched.terrain->art.size());size_t materialCells=0;
 for(size_t n=0;n<scalar.terrain->art.size();++n){const auto&a=scalar.terrain->art[n],&b=batched.terrain->art[n];
  auto fields=[](const auto&v){return std::tie(v.cell,v.type,v.overlayType,v.overlaySourceType,v.smudgeType,v.rubbleType,v.flags,v.pixelFX,v.land,v.smudgeData,v.overlay,v.sub,v.level,v.overlayFrame,v.sourceHidden,v.sourceColor);};
  assert(fields(a)==fields(b));materialCells+=bool(a.cell);
 }
 assert(batched.terrain->art[firstIndex].land==7&&batched.terrain->shade[firstIndex].shroud==-1);
 assert(batched.calls*4<scalar.calls);
 std::printf("PASS actual watch adapter + Sampler + ProcessReader: 128x128 map, %u physical Cells, %zu material Cells, eight complete fresh sweeps; old std::function RPM=%llu ms=%.3f, typed adapter RPM=%llu ms=%.3f; all colors/shade/material identities equal\n",count,materialCells,(unsigned long long)scalar.calls,scalar.ms,(unsigned long long)batched.calls,batched.ms);
 // A speculative prefetch spanning an unreadable page must not certify a
 // partial terrain or discard the last complete bank. Recovery re-reads it.
 {ProcessReader reader;assert(reader.open(GetCurrentProcessId()));Read backend{&reader,true};radar::Sampler<watchscene::ReadView<Read>> sampler({&backend});radar::Frame seed;seed.terrain=batched.terrain;seed.frame=8;sampler.useTerrain(seed);
  DWORD prior=0,ignored=0;assert(VirtualProtect((void*)(uintptr_t)Objects,4096,PAGE_NOACCESS,&prior));reader.begin();assert(!sampler.step(0,16384,true));assert(sampler.terrainFrame(8).terrain==seed.terrain&&reader.prefetchMisses);
  assert(VirtualProtect((void*)(uintptr_t)Objects,4096,prior,&ignored));const unsigned revision=sampler.revision();do{reader.begin();assert(sampler.step(0,16384,true));}while(sampler.revision()==revision);
  assert(sampler.terrainFrame(8).terrain->colors==seed.terrain->colors&&sampler.terrainFrame(8).terrain->shade==seed.terrain->shade);
  std::puts("PASS typed watch reader: failed speculative batch uses checked fallback, required unreadable Cell rejects candidate and retains complete bank; restored page recovers.");
 }
 assert(VirtualFree(arena,0,MEM_RELEASE));
}
