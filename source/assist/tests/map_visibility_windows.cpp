// Actual production callback and Service, synthetic gate in this own process.
// World/request flags are explicit seams; no game process is opened.
#define main baselineTechnologyFixtureMain
#include "tech_ui_breakpoint_windows.cpp"
#undef main
#include "../source/map_visibility_parameter_policy.h"
#include <initializer_list>
namespace mapvisibility=assist::mapvisibility;
static constexpr uint32_t ExperimentalReveal=0x04000000;
struct MapFixtureShared{uint32_t mapVisibility=1,stop=0,renderReveal=0,flags=0;}mapWire,*sh=&mapWire;
static bool fault=false;
namespace techparameters {
 static struct{LONG showing=1;}policy;static LONG mapExceptions=0;static bool worldReady=true;
 static bool world(){return worldReady;}
}
#include MAP_CALLBACK_EXTRACT
static bool mapCallback(void*raw,uint32_t tag){return tag==1&&mapVisibilityParameter(*static_cast<CONTEXT*>(raw));}
extern "C" __declspec(naked) int __stdcall CallMapGate(void*){__asm {
 push ebx
 push edi
 mov eax,[esp+12]
 mov edi,-1
 mov edx,6D88F4h
 call edx
 mov eax,edi
 pop edi
 pop ebx
 ret 4
}}
extern "C" __declspec(noinline) void MapSibling0(){MemoryBarrier();}
extern "C" __declspec(noinline) void MapSibling2(){MemoryBarrier();}
extern "C" __declspec(noinline) void MapSibling3(){MemoryBarrier();}
extern "C" __declspec(noinline) void MapSibling4(){MemoryBarrier();}
static bool sameMapDebug(const CONTEXT&a,const CONTEXT&b){return a.Dr0==b.Dr0&&a.Dr1==b.Dr1&&a.Dr2==b.Dr2&&a.Dr3==b.Dr3&&a.Dr7==b.Dr7;}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);setbuf(stdout,nullptr);unsigned checks=0;
 auto check=[&](bool okay){assert(okay);++checks;};
 auto*page=VirtualAlloc(reinterpret_cast<void*>(0x6D0000),65536,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);check(page==reinterpret_cast<void*>(0x6D0000));
 auto*code=reinterpret_cast<uint8_t*>(assist::mapvisibility::PrefixAddress);
 memcpy(code,assist::mapvisibility::Prefix,sizeof assist::mapvisibility::Prefix);
 // Fixture continuation: EDI=-2; EBX=8; ret. Not a game dump or game patch.
 const uint8_t suffix[]={0xBF,0xFE,0xFF,0xFF,0xFF,0xBB,8,0,0,0,0xC3};memcpy(code+10,suffix,sizeof suffix);
 FlushInstructionCache(GetCurrentProcess(),code,21);uint8_t originalCode[21]{};memcpy(originalCode,code,21);
 alignas(4) uint8_t cell[0x130]{};auto&bits=*reinterpret_cast<uint32_t*>(cell+0x12C);
 for(uint32_t value:{0u,8u,16u,24u,128u,255u}){bits=value;check(CallMapGate(cell)==(value&24?-1:-2)&&bits==value);}
 CONTEXT original=debug();assist::techbp::Service service;
 const uint32_t sites[5]={uint32_t(uintptr_t(&MapSibling0)),assist::mapvisibility::Address,uint32_t(uintptr_t(&MapSibling2)),uint32_t(uintptr_t(&MapSibling3)),uint32_t(uintptr_t(&MapSibling4))};
 check(service.configure(sites,2,mapCallback));wait(service);check(service.activeTags()==2&&__builtin_popcount(service.lease.mask)==1);
 for(uint32_t value:{0u,8u,16u,24u,128u,255u}){bits=value;check(CallMapGate(cell)==-1&&bits==value);}
 // Exercise every production permission guard while a real DR remains installed.
 bits=0;sh->mapVisibility=0;check(CallMapGate(cell)==-2);sh->mapVisibility=1;
 techparameters::policy.showing=0;check(CallMapGate(cell)==-2);techparameters::policy.showing=1;
 techparameters::worldReady=false;check(CallMapGate(cell)==-2);techparameters::worldReady=true;
 sh->stop=1;check(CallMapGate(cell)==-2);sh->stop=0;fault=true;check(CallMapGate(cell)==-2);fault=false;
 sh->renderReveal=1;check(CallMapGate(cell)==-2);sh->renderReveal=0;sh->flags=ExperimentalReveal;check(CallMapGate(cell)==-2);sh->flags=0;
 CONTEXT c{};c.Eip=assist::mapvisibility::Address;c.EFlags=0x10AD7;const CONTEXT before=c;
 check(mapVisibilityParameter(c));CONTEXT expected=before;expected.EFlags&=~0x40u;check(!memcmp(&c,&expected,sizeof c));
 code[0]=0x90;c=before;check(!mapVisibilityParameter(c)&&!memcmp(&c,&before,sizeof c));code[0]=originalCode[0];
 // Three existing groups + Map use four slots. A fifth request is rejected
 // transactionally, preserving the existing register ownership and callback.
 check(service.configure(sites,23,mapCallback));wait(service);check(service.activeTags()==23&&assist::slotbudget::owned(service.slotBudget())==4);
 auto registersBefore=debug();check(service.configure(sites,31,mapCallback));wait(service);check(service.state==assist::techbp::Slots&&service.activeTags()==23&&sameMapDebug(registersBefore,debug()));check(CallMapGate(cell)==-1);
 check(service.configure(sites,29,mapCallback));wait(service);check(service.activeTags()==29&&CallMapGate(cell)==-2&&!assist::slotbudget::allows(service.slotBudget(),31));
 registersBefore=debug();check(service.configure(sites,31,mapCallback));wait(service);check(service.state==assist::techbp::Slots&&service.activeTags()==29&&sameMapDebug(registersBefore,debug()));
 retire(service);check(sameMapDebug(original,debug())&&!memcmp(code,originalCode,21));
 check(VirtualFree(page,0,MEM_RELEASE)!=FALSE);
 std::printf("{\"pass\":true,\"checks\":%u,\"real_own_process_DR_used\":true,\"production_callback_extracted\":true,\"world_is_seam\":true,\"game_process_accessed\":false,\"synthetic_cell_and_code_unchanged_after_calls\":true}\n",checks);
}
