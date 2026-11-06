#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#ifdef NDEBUG
#error This fixture must execute every assertion; compile with -UNDEBUG.
#endif
// Own-process fixed-address data only, no game executable or engine calls.
// The loader maps this test-only BSS before choosing CRT heap/stack addresses.
// Its runner fixes only this fixture PE at 0x400000, following the existing
// sidebar_view_guard_seh fixture. Every target must belong to this actual array.
static volatile uint8_t fixtureArena[0x800000];
struct FixtureShared {volatile uint32_t stop=0,heartbeat=0,singlePlayerFPS=0,singlePlayerFPSActive=0,singlePlayerFPSHits=0;};
static FixtureShared state;static FixtureShared*sh=&state;static bool fault=false;
static DWORD sampleClock(FixtureShared*){return GetTickCount();}
template<class T>static T get(uint32_t address){return *reinterpret_cast<T*>(address);}
static bool read(uint32_t p,void*out,size_t n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&got)&&got==n;}
#include "../source/single_player_fps_runtime.h"
using namespace assist;
template<class T>static void put(uint32_t at,T value){*reinterpret_cast<T*>(at)=value;}
static void* page(uint32_t p,size_t n){
 void*module=reinterpret_cast<void*>(GetModuleHandleW(nullptr));
 const uint64_t begin=uintptr_t(fixtureArena),arenaEnd=begin+sizeof fixtureArena,end=uint64_t(p)+n;
 const bool inside=module==reinterpret_cast<void*>(0x400000)&&n&&end>p&&p>=begin&&end<=arenaEnd&&arenaEnd<=0x100000000ULL;
 if(!inside)fprintf(stderr,"FPS fixture outside owned BSS: target=0x%08lx bytes=0x%lx arena=0x%08lx..0x%08lx module=%p\n",static_cast<unsigned long>(p),static_cast<unsigned long>(n),static_cast<unsigned long>(begin),static_cast<unsigned long>(arenaEnd),module);
 assert(inside);
 for(uint64_t cursor=p;cursor<end;){
  MEMORY_BASIC_INFORMATION region{};
  const SIZE_T queried=VirtualQuery(reinterpret_cast<void*>(uintptr_t(cursor)),&region,sizeof region);
  const uint64_t regionEnd=uint64_t(uintptr_t(region.BaseAddress))+region.RegionSize;
  const DWORD protection=region.Protect&0xff;
  const bool writable=protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
  const bool owned=queried==sizeof region&&region.State==MEM_COMMIT&&region.Type==MEM_IMAGE&&region.AllocationBase==module&&
   !(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&writable&&uint64_t(uintptr_t(region.BaseAddress))<=cursor&&regionEnd>cursor;
  if(!owned)fprintf(stderr,"FPS fixture ownership rejected: address=0x%08lx state=0x%lx type=0x%lx protect=0x%lx owner=%p\n",static_cast<unsigned long>(cursor),region.State,region.Type,region.Protect,region.AllocationBase);
  assert(owned);cursor=regionEnd<end?regionEnd:end;
 }
 printf("Verified owned fixture BSS: 0x%08lx + 0x%lx\n",static_cast<unsigned long>(p),static_cast<unsigned long>(n));
 return reinterpret_cast<void*>(uintptr_t(p));
}
static const uint8_t prefix[]={0x8b,0x35,0x60,0xeb,0xa8,0,0x8d,0x4c,0x24,0x14,0xe8,0x93,0xb4,0x16,0,0x8b,0x4c,0x24,0x14,0xa3,0x48,0x73,0x88,0,0x89,0x0d,0x4c,0x73,0x88,0,0x89,0x35,0x50,0x73,0x88,0};
static void setup(unsigned fps=45,int mode=5){state={};state.heartbeat=GetTickCount();state.singlePlayerFPS=fps;put(0xA8B238,mode);put(0xA8B230,0x18000000u);put(0xA8ED84,100);put(0xA8E9A0,uint8_t(1));put(0x887350,7u);put(0xA8EB60,3u);memcpy(reinterpret_cast<void*>(0x55D79E),prefix,sizeof prefix);singlefps::maintain();singlefps::enabled=singlefps::desired;}
static bool invoke(){CONTEXT c{};c.Eip=singlefps::Entry;c.Esi=7;c.EFlags=0x246;const bool applied=singlefps::parameter(c);assert(c.Esi==7&&c.EFlags==0x246);assert(c.Eip==(applied?singlefps::Resume:singlefps::Entry));assert(get<unsigned>(0x887350)==(applied?0u:7u));assert(get<unsigned>(0xA8EB60)==3);return applied;}
static DWORD WINAPI off(void*){Sleep(5);sh->singlePlayerFPS=0;return 0;}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 page(0x55D000,0x1000);page(0x887000,0x1000);page(0xA8B000,0x4000);
 setup();assert(singlefps::desired&&invoke()&&state.singlePlayerFPSActive==45);
 LARGE_INTEGER a{},b{},hz{};QueryPerformanceFrequency(&hz);QueryPerformanceCounter(&a);
 for(unsigned n=0;n<12;++n){state.heartbeat=GetTickCount();put(0x887350,7u);assert(invoke());}
 QueryPerformanceCounter(&b);const double ms=double(b.QuadPart-a.QuadPart)*1000/hz.QuadPart;
 assert(ms>=180&&ms<1500);printf("own-process 45 FPS deadline window, 12 waits: %.3f ms\n",ms);
 for(int s=0;s<10;++s)if(!singlefps::session(s)){setup(45,s);assert(!singlefps::desired&&singlefps::failure==-6);singlefps::enabled=true;assert(!invoke());}
 setup(45,0);assert(invoke());setup();state.stop=1;assert(!invoke());
 setup();state.heartbeat=GetTickCount()-501;assert(!invoke());
 setup();state.singlePlayerFPS=0;assert(!invoke());
 setup();fault=true;assert(!invoke());fault=false;
 setup();put(0xA8ED84,99);assert(!invoke());
 setup();put(0xA8B230,0x18001000u);assert(!invoke());
 setup();put(0x55D79E,uint8_t(0x90));assert(!invoke());
 setup(10);assert(invoke());put(0x887350,7u);HANDLE worker=CreateThread(nullptr,0,off,nullptr,0,nullptr);assert(worker);assert(!invoke());assert(WaitForSingleObject(worker,1000)==WAIT_OBJECT_0);CloseHandle(worker);
 singlefps::release();assert(!singlefps::timer&&!singlefps::enabled&&!singlefps::desired);
 puts("PASS: production FPS callback, native MOV destination/continuation, preserved registers/flags/speed, live SP-only gates, off during wait, cleanup; no game");
}
