#ifndef CAMERA_ASSETS_HEADER
#define CAMERA_ASSETS_HEADER "../../external_reader/source/camera_assets.h"
#endif
#include CAMERA_ASSETS_HEADER
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdio>

// Real ReadProcessMemory calls against synthetic, owned pages. This isolates
// production palette refresh/capture without a game, frame reuse or throttling.
constexpr uint32_t Objects=0x20000000,Types=0x21000000,Converter=0x22000000,Colors=0x22001000;
struct Region {uint32_t address;size_t size;void*memory;};
static std::array<Region,4> regions{{{0x800000,0x400000,nullptr},{Objects,0x400000,nullptr},{Types,0x10000,nullptr},{Converter,0x10000,nullptr}}};
static void* address(uint32_t p,size_t n){for(const auto&r:regions)if(r.memory&&p>=r.address&&uint64_t(p)+n<=uint64_t(r.address)+r.size)return static_cast<uint8_t*>(r.memory)+(p-r.address);return nullptr;}
template<class T>static void put(uint32_t p,T value){auto out=address(p,sizeof value);assert(out);std::memcpy(out,&value,sizeof value);}
struct Pages {
 // Translate the synthetic 32-bit address space to ASLR-safe owned pages;
 // every successful read below still calls the real Windows RPM API.
 Pages(){for(auto&r:regions){r.memory=VirtualAlloc(nullptr,r.size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(r.memory);}}
 ~Pages(){for(auto&r:regions){if(r.memory)VirtualFree(r.memory,0,MEM_RELEASE);r.memory=nullptr;}}
};
struct Reader {
 uint64_t calls=0,bytes=0,palettes=0;uint32_t failAt=0,raceObject=0,raceBulkAt=0;bool racePalette=false,raceBulk=false;
 bool imagePath(wchar_t*,DWORD*){return false;}
 bool readFresh(uint32_t p,void*out,size_t n){++calls;bytes+=n;
  if(failAt&&p<=failAt&&uint64_t(failAt)<uint64_t(p)+n)return false;
  auto memory=address(p,n);SIZE_T got=0;if(!memory||!ReadProcessMemory(GetCurrentProcess(),memory,out,n,&got)||got!=n)return false;
  if(p==Colors&&n==768){++palettes;if(racePalette&&(palettes&1)==0)static_cast<uint8_t*>(out)[17]^=1;
   if(raceObject){put<uint32_t>(raceObject+0x10,0xFFFFFFFF);raceObject=0;}}
  if(raceBulk&&p>=Objects&&p<Objects+0x400000&&n>256){put<uint32_t>(p+0x10,0xFFFFFFFF);raceBulk=false;}
  if(raceBulkAt&&p==raceBulkAt&&n>256){put<uint32_t>(p+0x10,0xFFFFFFFF);raceBulkAt=0;}
  return true;
 }
 void reset(){calls=bytes=palettes=0;}
};
using Context=external::cameraassets::Context<Reader>;
static Context::ActorArt actor(unsigned index,int kind){
 Context::ActorArt a;a.object=Objects+index*0x400;a.id=index+1;a.kind=kind;a.type=Types+uint32_t(kind)*0x1000;
 const uint32_t p=a.object,t=a.type;put<uint32_t>(p+0x10,a.id);put<uint8_t>(p+0x90,1);put<int>(p+0x9C,2560);put<int>(p+0xA0,5120);
 if(kind==5){put<uint32_t>(p,0x7E3354);put<uint32_t>(p+0xC8,t);put<uint32_t>(p+0xD4,Converter);put<uint8_t>(p+0xDC,'X');put<int>(t+0x364,2);
  if(index<240){a.parent=Objects+0x100000+index*0x800;a.parentId=index+10000;put<uint32_t>(a.parent,0x7E3EBC);put<uint32_t>(a.parent+0x10,a.parentId);put<int>(a.parent+0x6C,100);put<uint32_t>(a.parent+0x55C,p);}
 }
 else if(kind==7){put<uint32_t>(p,external::particles::Vtable);put<uint32_t>(p+0xAC,t);put<int>(t+0x314,0);}
 else {put<uint32_t>(p,external::projectiles::Vtable);put<uint32_t>(p+0xAC,t);put<uint32_t>(t,external::projectiles::TypeVtable);put<uint8_t>(t+0x2A1,1);}
 return a;
}
static uint64_t digest=1469598103934665603ull;
static void hash(const std::array<uint32_t,256>&values){for(auto v:values){digest^=v;digest*=1099511628211ull;}}
static void palette(unsigned frame){for(unsigned i=0;i<768;++i)put<uint8_t>(Colors+i,uint8_t((i*13+frame*19)%255+1));}
int main(){
 Pages pages;Reader r;Context c(r);put<uint32_t>(Converter,0x7ED0A4);put<uint32_t>(Converter+0x188,Colors);put<uint32_t>(0x87F6C0,Converter);put<uint32_t>(0x87F6C4,Converter);
 std::vector<Context::ActorArt> actors;for(unsigned i=0;i<800;++i)actors.push_back(actor(i,i<480?5:i<640?7:6));
 unsigned checks=0;auto check=[&](bool value){assert(value);++checks;};
 for(unsigned f=0;f<6;++f){palette(f);c.beginPresentation();for(auto&a:actors){bool changed=false;check(c.refreshActorPalette(a,changed)&&changed);hash(a.colors);auto rgb=static_cast<const uint8_t*>(address(Colors+3,3));check(a.colors[1]==(0xFF000000u|uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2]));}}
 // Each freshness pass still compares two independently read palettes.
 for(unsigned index:{0u,480u,640u}){auto&a=actors[index];auto old=a.colors;auto revision=a.paletteRevision;bool changed=false;
  r.racePalette=true;r.palettes=0;check(!c.refreshActorPalette(a,changed)&&a.colors==old&&a.paletteRevision==revision);r.racePalette=false;
  r.failAt=Colors+37;check(!c.refreshActorPalette(a,changed)&&a.colors==old);r.failAt=0;
  r.raceObject=a.object;check(!c.refreshActorPalette(a,changed)&&a.colors==old);put<uint32_t>(a.object+0x10,a.id);
  r.failAt=a.object+0x90;check(!c.refreshActorPalette(a,changed)&&a.colors==old);r.failAt=0;
  // An unreadable padding byte makes the speculative bulk fail, but none of
  // capture's required fields touch it. The original field path must succeed.
  r.failAt=a.object+0x40;check(c.refreshActorPalette(a,changed)&&a.colors==old);r.failAt=0;
  check(c.refreshActorPalette(a,changed));
 }
 {auto&a=actors[0];auto old=a.colors;bool changed=false;r.raceObject=a.parent;check(!c.refreshActorPalette(a,changed)&&a.colors==old);put<uint32_t>(a.parent+0x10,a.parentId);
  r.failAt=a.parent+0x55C;check(!c.refreshActorPalette(a,changed)&&a.colors==old);r.failAt=0;check(c.refreshActorPalette(a,changed));}
#ifndef PALETTE_BASELINE
 for(unsigned index:{0u,480u,640u}){auto&a=actors[index];auto old=a.colors;bool changed=false;r.raceBulk=true;check(!c.refreshActorPalette(a,changed)&&a.colors==old);check(!r.raceBulk);put<uint32_t>(a.object+0x10,a.id);}
 {auto&a=actors[0];auto old=a.colors;bool changed=false;r.raceBulkAt=a.parent;check(!c.refreshActorPalette(a,changed)&&a.colors==old);check(!r.raceBulkAt);put<uint32_t>(a.parent+0x10,a.parentId);}
 // Partial overlaps and reads extending beyond a batch must not later expose
 // the original block again. Every repeat observation returns to the source.
 {auto source=[&](uint32_t p,void*out,size_t n){return r.readFresh(p,out,n);};external::fieldreads::First<decltype(source)> batch(source);constexpr uint32_t p=Converter+0x3000;batch.block(p,16);auto read=batch.view();uint32_t value=0;
  auto calls=r.calls;check(read(p,&value,4)&&r.calls==calls);put<uint32_t>(p+2,0x12345678);check(read(p+2,&value,4)&&value==0x12345678&&r.calls==calls+1);
  uint16_t half=0;check(read(p+4,&half,2)&&half==0x1234&&r.calls==calls+2);uint8_t tail[8]{};check(read(p+12,tail,8)&&r.calls==calls+3);check(read(p+12,&value,4)&&r.calls==calls+4);
  external::fieldreads::First<decltype(source)> overlap(source);overlap.block(p,16);overlap.block(p+8,16);auto both=overlap.view();uint8_t across[20]{};calls=r.calls;
  check(both(p+4,across,sizeof across)&&r.calls==calls+1);put<uint32_t>(p+8,0xAABBCCDD);check(both(p+8,&value,4)&&value==0xAABBCCDD&&r.calls==calls+2);
 }
#endif
 std::vector<double> times;uint64_t reads=0,bytes=0,palettes=0;
 for(unsigned trial=0;trial<5;++trial){r.reset();auto start=std::chrono::steady_clock::now();
  for(unsigned f=0;f<8;++f){palette(10+f);c.beginPresentation();for(auto&a:actors){bool changed=false;assert(c.refreshActorPalette(a,changed)&&changed);hash(a.colors);}}
  auto finish=std::chrono::steady_clock::now();times.push_back(std::chrono::duration<double,std::milli>(finish-start).count()/8);reads=r.calls/8;bytes=r.bytes/8;palettes=r.palettes/8;
 }
 std::sort(times.begin(),times.end());std::printf("{\"actors\":%zu,\"checks\":%u,\"readsPerFrame\":%llu,\"bytesPerFrame\":%llu,\"paletteReadsPerFrame\":%llu,\"medianMs\":%.6f,\"digest\":\"%016llx\"}\n",actors.size(),checks,(unsigned long long)reads,(unsigned long long)bytes,(unsigned long long)palettes,times[2],(unsigned long long)digest);
}
