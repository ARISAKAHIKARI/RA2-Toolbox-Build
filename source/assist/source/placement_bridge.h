// Explicit experimental game-judgment opt-ins. Sole writer: GameEntryManager.
#include "../../shared/source/experimental_patch_policy.h"
extern "C" int __cdecl GuardedRiskAction(unsigned,unsigned,unsigned);
static bool riskPlacementActive=false,riskAirActive=false,riskRenderActive=false;
static bool riskReady(){
 if(!gameentry::active(gameentry::Client::Assist)||!sh||sh->stop||fault||!lastHouse||
    DWORD(sampleClock(sh)-sh->heartbeat)>=3000||!get<bool>(0xA8E9A0))return false;
 // Native callbacks see the current frame: equality with the last message
 // snapshot would make the behavior disappear between host samples.
 const uint32_t house=get<uint32_t>(0xA83D4C);const int frame=get<int>(0xA8ED84);
 return house==lastHouse&&house!=get<uint32_t>(0xAC1198)&&!get<bool>(house+0x1F5)&&
   !get<bool>(house+0x1F6)&&!get<bool>(house+0x1F8)&&frame>=snap.frame&&frame<INT32_MAX&&
   get<int>(0xA8B238)==snap.session&&eventdelivery::mayQueue(snap.session);
}
extern "C" int __cdecl AssistRiskAction(unsigned op,unsigned object,unsigned house){
 if(!riskReady())return 0;
 if(op==3)return riskRenderActive&&sh->renderReveal!=0; // draw-only, no native game calls
 if(op==0)return riskPlacementActive&&(sh->flags&BuildAnywhere)&&house==get<uint32_t>(lastHouse+0x30)&&object>=0x10000&&
   call<int>(get<uint32_t>(get<uint32_t>(object)+0x2C),object)==7;
 if(!riskAirActive||!(sh->flags&NativeAirWaypoints))return 0;
 if(op==1)return 1; // AircraftType::CanUseWaypoint, the native UI gate.
 if(op!=2||!own(object,lastHouse)||kind(object)!=0||get<uint32_t>(object+0x5D4)||
    get<uint32_t>(object+0x294)||get<uint8_t>(object+0x6CA))return 0;
 // YRpp TechnoClass::TryNextPlanningTokenNode: native planning owns its tokens
 // and event construction. No external queue, mission/target write, OutList
 // removal, or second frame scheduler.
 return call<bool>(0x6385C0,object)?1:0;
}
// Preserve flags, registers and x87/SSE state. Replay displaced instructions
// on the original path. No generated executable allocation/trampoline.
static __declspec(naked) void riskPlacementEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 mov eax,[ebx+12]
 push dword ptr [eax+12]
 push dword ptr [eax+8]
 push 0
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,1
 ret 16
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov eax,dword ptr ds:[0A83D4Ch]
 push 4A8EB5h
 ret
}}
static __declspec(naked) void riskAirTypeEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 push 0
 push ecx
 push 1
 call GuardedRiskAction
 add esp,12
 mov [ebx+28],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 ret
}}
static __declspec(naked) void riskAirArrivalEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 push 0
 push esi
 push 2
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 416AC0h
 ret
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov dword ptr [esi+0BCh],3
 push 416AC0h
 ret
}}
// Only the final shroud/fog drawing leaf is bypassed. The caller retains
// GetOcclusion and its original Visibility/Foggedness cache updates.
static __declspec(naked) void riskShroudEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 push 0
 push 0
 push 3
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 ret 12
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push ebp
 mov ebp,esp
 sub esp,34h
 push 47EFE6h
 ret
}}
// Only the final shroud/fog drawing leaf is bypassed. The caller retains
// GetOcclusion and its original Visibility/Foggedness cache updates.
static __declspec(naked) void riskFogEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 push 0
 push 0
 push 3
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 ret 12
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,byte ptr ds:[089E7C6h]
 push 47F255h
 ret
}}
struct RiskSite {uint32_t address;unsigned size;uint8_t before[16],after[16];};
static RiskSite riskSites[]={
 {experimentalpatch::Placement,5,{0xA1,0x4C,0x3D,0xA8,0},{}},
 {experimentalpatch::AirType,4,{0x50,0xCB,0x41,0},{}},
 {experimentalpatch::AirArrival,10,{0xC7,0x86,0xBC,0,0,0,3,0,0,0},{}},
 {experimentalpatch::ShroudDraw,6,{0x55,0x8B,0xEC,0x83,0xEC,0x34},{}},
 {experimentalpatch::FogDraw,5,{0xA0,0xC6,0xE7,0x89,0},{}}
};
static void prepareRiskSites(){static bool ready=false;if(ready)return;ready=true;
 const uint32_t targets[]={uint32_t(uintptr_t(&riskPlacementEntry)),uint32_t(uintptr_t(&riskAirTypeEntry)),uint32_t(uintptr_t(&riskAirArrivalEntry)),uint32_t(uintptr_t(&riskShroudEntry)),uint32_t(uintptr_t(&riskFogEntry))};
 for(unsigned i=0;i<5;++i){auto&s=riskSites[i];
  if(s.size==4)memcpy(s.after,&targets[i],4);
  else{memset(s.after,0x90,s.size);s.after[0]=0xE9;uint32_t offset=targets[i]-s.address-5;memcpy(s.after+1,&offset,4);}
 }
}
static bool riskSiteCurrent(const RiskSite&s){uint8_t bytes[16]{};return read(s.address,bytes,s.size)&&!memcmp(bytes,s.after,s.size);}
static bool restoreRiskSite(RiskSite&s){
 if(!codeHookRegistry.active(s.address))return true;
 return gameentry::patch(gameentry::Client::Assist,s.address,s.after,s.before,s.size);
}
static bool installRiskSite(RiskSite&s){
 if(codeHookRegistry.active(s.address)){if(riskSiteCurrent(s))return !codeHookRegistry.conflict(s.address);codeHookRegistry.markConflict(s.address);return false;}
 return gameentry::patch(gameentry::Client::Assist,s.address,s.before,s.after,s.size);
}
static void restorePlacement(){
 riskPlacementActive=riskAirActive=riskRenderActive=false;prepareRiskSites();
 bool place=restoreRiskSite(riskSites[0]);
 bool type=restoreRiskSite(riskSites[1]),arrival=restoreRiskSite(riskSites[2]);
 snap.placementStatus=place?0:-2;snap.nativeAirStatus=type&&arrival?0:-2;
 bool shroud=restoreRiskSite(riskSites[3]),fog=restoreRiskSite(riskSites[4]);
 snap.renderRevealStatus=shroud&&fog?0:-2;
}
static void redrawRevealTest(){
 // Native redraw bookkeeping only, once per toggle; never reveal/register cells.
 static const uint8_t signature[]={0x83,0xEC,0x58,0x8B,0x0D,0x88,0xCE,0xB0,0};uint8_t actual[sizeof signature]{};
 const uint32_t tactical=get<uint32_t>(0x887324);
 struct Area {int x,y,w,h;};Area area{};
 if(tactical<0x10000||!riskReady()||!read(0x6D2790,actual,sizeof actual)||memcmp(actual,signature,sizeof actual)||!read(0x886FA0,&area,sizeof area)||area.w<=0||area.h<=0||area.w>16384||area.h>16384)return;
 call<void>(0x6D2790,tactical,area,false);
}
static void placementTick(){
 prepareRiskSites();
 bool priorRender=riskRenderActive;
 if(sh->renderReveal){
  riskRenderActive=installRiskSite(riskSites[3])&&installRiskSite(riskSites[4]);
  if(!riskRenderActive){restoreRiskSite(riskSites[3]);restoreRiskSite(riskSites[4]);}
  snap.renderRevealStatus=riskRenderActive?1:-2;
 }else{riskRenderActive=false;bool a=restoreRiskSite(riskSites[3]),b=restoreRiskSite(riskSites[4]);snap.renderRevealStatus=a&&b?0:-2;}
 if(priorRender!=riskRenderActive)redrawRevealTest();
 if(sh->flags&BuildAnywhere){riskPlacementActive=installRiskSite(riskSites[0]);snap.placementStatus=riskPlacementActive?1:-2;}
 else{riskPlacementActive=false;snap.placementStatus=restoreRiskSite(riskSites[0])?0:-2;}
 if(sh->flags&NativeAirWaypoints){
  static const uint8_t nextSignature[]={0x81,0xEC,0xE0,0,0,0,0x53,0x56,0x8B,0xF1};uint8_t actual[sizeof nextSignature]{};
  bool supported=read(0x6385C0,actual,sizeof actual)&&!memcmp(actual,nextSignature,sizeof actual);
  // Behavior first, frontend last. Partial installation rolls back as a group.
  riskAirActive=supported&&installRiskSite(riskSites[2])&&installRiskSite(riskSites[1]);
  if(!riskAirActive){restoreRiskSite(riskSites[1]);restoreRiskSite(riskSites[2]);}
  snap.nativeAirStatus=riskAirActive?1:supported?-2:-1;
 }else{riskAirActive=false;bool a=restoreRiskSite(riskSites[1]),b=restoreRiskSite(riskSites[2]);snap.nativeAirStatus=a&&b?0:-2;}
}
