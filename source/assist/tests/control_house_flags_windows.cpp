#define WIN32_LEAN_AND_MEAN
// Run this fixture as x64: the x86 CRT may occupy the fixed 0xA80000 mock
// globals. All simulated game pointers remain uint32_t below 2 GiB.
#define NOMINMAX
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <windows.h>
#include <stdint.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../source/control_mode_policy.h"
using namespace assist;
namespace gameentry {enum class Client {Assist};static bool active(Client){return true;}}
struct Shared {uint32_t controlAll=0;int playerControlStatus=0;};
static Shared transport,*sh=&transport;
static struct {uint32_t epoch=1;} sessionTracker;
static bool ready=true,retired=true;
static uint32_t readFailure=0;
template<class T>static T get(uint32_t at){return *(const T*)(uintptr_t)at;}
template<class T>static void put(uint32_t at,T value){*(T*)(uintptr_t)at=value;}
static bool read(uint32_t at,void*out,size_t size){
 if(readFailure>=at&&readFailure<uint64_t(at)+size)return false;
 SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(const void*)(uintptr_t)at,out,size,&got)&&got==size;
}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t at,int){Vec v{};return read(at,&v,sizeof v)?v:Vec{};}
static bool playableHouse(uint32_t p){return p==0x10000000||p==0x10010000;}
#include "../source/control_house_flags_bridge.h"
static bool riskReady(bool){return ready;}
static bool controlCodeRetired(){return retired;}
int main(){
 assert(VirtualAlloc((void*)0xA80000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void*)0xA80000);
 assert(VirtualAlloc((void*)0x10000000,0x30000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void*)0x10000000);
 const Vec v{0,0x10020000,2,0,2,1};put<Vec>(0xA80228,v);
 put<uint32_t>(0x10020000,0x10000000);put<uint32_t>(0x10020004,0x10010000);
 put<uint32_t>(0xA8B230,0x10021000);put<int>(0xA8B238,3);put<int>(0xA8ED84,100);
 put<uint32_t>(0xA83D4C,0x10000000);
 for(unsigned i=0;i<2;++i){const uint32_t p=0x10000000+i*0x10000;
  put<uint32_t>(p,0x7E0000);put<uint32_t>(p+0x10,100+i);put<int>(p+0x30,i);put<uint32_t>(p+0x34,0x10022000+i*4);
  put<uint8_t>(p+0x1EC,0);put<uint8_t>(p+0x1ED,0);put<uint32_t>(p+0x21C,0xC0FFEE);
 }
 sh->controlAll=controlmode::Native;controlHouseFlagTick();assert(!controlHouseFlagsPending());
 sh->controlAll=controlmode::HouseFlag;retired=false;controlHouseFlagTick();
 assert(sh->playerControlStatus==-2&&!controlHouseFlagsPending()&&get<uint8_t>(0x100001ED)==0);
 retired=true;controlHouseFlagTick();assert(sh->playerControlStatus==2&&controlHouseFlagsPending());
 assert(get<uint8_t>(0x100001ED)==1&&get<uint8_t>(0x100101ED)==1);
 assert(get<uint8_t>(0x100001EC)==0&&get<uint32_t>(0x1000021C)==0xC0FFEE&&get<uint32_t>(0xA83D4C)==0x10000000);
 // Focus/simulation suspension keeps ownership and does not change data.
 ready=false;controlHouseFlagMaintenance();controlHouseFlagTick();assert(controlHouseFlagsPending()&&get<uint8_t>(0x100001ED)==1);ready=true;
 // Native mode may not start until an actual Win32 protected-page failure
 // has recovered. A sibling record restores independently in the meantime.
 DWORD previous=0;assert(VirtualProtect((void*)0x10000000,0x1000,PAGE_READONLY,&previous));
 sh->controlAll=controlmode::Native;controlHouseFlagMaintenance();
 assert(controlHouseFlagsPending()&&sh->playerControlStatus==-2&&get<uint8_t>(0x100101ED)==0);
 assert(VirtualProtect((void*)0x10000000,0x1000,previous,&previous));controlHouseFlagMaintenance();
 assert(!controlHouseFlagsPending()&&get<uint8_t>(0x100001ED)==0);
 sh->controlAll=controlmode::HouseFlag;controlHouseFlagTick();assert(sh->playerControlStatus==2);
 // Temporary read failure cannot acknowledge cleanup or forget originals.
 sh->controlAll=controlmode::Off;readFailure=0x10000010;controlHouseFlagMaintenance();assert(controlHouseFlagsPending());
 readFailure=0;controlHouseFlagMaintenance();assert(!controlHouseFlagsPending());
 // A local-player/command-epoch change does not prove the House disappeared.
 sh->controlAll=controlmode::HouseFlag;controlHouseFlagTick();++sessionTracker.epoch;controlHouseFlagMaintenance();
 assert(!controlHouseFlagsPending()&&get<uint8_t>(0x100001ED)==0);
 controlHouseFlagTick();assert(sh->playerControlStatus==2);
 assert(restoreControlHouseFlags()&&!controlHouseFlagsPending());
 puts("PASS Win32 data bridge: mode and pending-code exclusion, real writable/protected House pages, focus retention, partial cleanup, retry and epoch migration; no game launched");
}
