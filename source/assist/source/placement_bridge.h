// Explicit experimental game-judgment opt-ins. Sole writer: GameEntryManager.
#include "../../shared/source/experimental_patch_policy.h"
#include "../../shared/source/patch_context.h"
extern "C" int __cdecl GuardedRiskAction(unsigned,unsigned,unsigned);
static bool riskControlActive;
static bool riskPlacementActive=false,riskAirActive=false,riskRenderActive=false,riskC4Active=false;
static bool riskRenderRedrawPending=false;
static bool riskContextReady(bool captured=false){
 if((!captured&&!gameentry::active(gameentry::Client::Assist))||fault||!lastHouse||!get<bool>(0xA8E9A0))return false;
 // Native callbacks see the current frame: equality with the last message
 // snapshot would make the behavior disappear between host samples.
 const uint32_t house=get<uint32_t>(0xA83D4C);const int frame=get<int>(0xA8ED84);
 return house==lastHouse&&house!=get<uint32_t>(0xAC1198)&&!get<bool>(house+0x1F5)&&
   !get<bool>(house+0x1F6)&&!get<bool>(house+0x1F8)&&frame>=snap.frame&&frame<INT32_MAX&&
   get<int>(0xA8B238)==snap.session&&eventdelivery::mayQueue(snap.session)&&
   external::display::inspect([](uint32_t at,void*out,size_t n){memcpy(out,(void*)(uintptr_t)at,n);return true;}).visible();
}
static bool riskReady(bool captured=false){
 if(!sh||sh->stop)return false;
 const DWORD heartbeat=sh->heartbeat; // Capture before the clock; host writes concurrently.
 return DWORD(sampleClock(sh)-heartbeat)<3000&&riskContextReady(captured);
}
#include "shroud_render_bridge.h"
static int smallMapCameraAction(uint32_t),highScrollAction(uint32_t);
extern "C" int __cdecl AssistRiskAction(unsigned op,unsigned object,unsigned house){
 // Only GuardedRiskAction calls this helper, after obtaining a Capture lease.
 // Keep live world checks, but don't re-enter Validate on every drawn cell.
 if(op==8)return riskReady(true)?highScrollAction(object):0;
 if(op==7)return riskReady(true)?smallMapCameraAction(object):0;
 if(op==5)return techGate(object,house);
 if(op==3&&(!riskRenderActive||!sh||!sh->renderReveal))return 0;
 if(!riskReady(true)){if(op==3)riskRenderRedrawPending=true;return 0;}
 if(op==6)return riskControlActive&&sh->controlAll&&playableHouse(house);
 if(op==4)return 0; // retired: preserve native movement and C4 capability checks
 if(op==3)return sh->renderReveal==2?halfShroud(object,house!=0):1;
 if(op==0)return riskPlacementActive&&(sh->flags&BuildAnywhere)&&house==get<uint32_t>(lastHouse+0x30)&&object>=0x10000&&
   call<int>(get<uint32_t>(get<uint32_t>(object)+0x2C),object)==7;
 if(!riskAirActive||!airpathpolicy::native(get<int>(0xA8B238),(sh->flags&NativeAirWaypoints)!=0))return 0;
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
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
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
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
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
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
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
// Enter AFTER the original resource initialization, including platform code.
// The old function-entry return also skipped lazy resource initialization.
// GetOcclusion/cache updates and the complete resource prefix now still run.
static __declspec(naked) void riskShroudEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push ebp
 push 3
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov esp,ebp
 pop ebp
 ret 12
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov eax,[ebp+10h]
 push ebx
 push esi
 push 47F03Dh
 ret
}}
// Fog likewise keeps its initialization prefix and the original 0x2C locals.
static __declspec(naked) void riskFogEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 1
 lea eax,[ebx+36]
 push eax
 push 3
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 lea esp,[esp+2Ch]
 ret 12
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov eax,[esp+38h]
 push ebx
 push 47F279h
 ret
}}
// Replace only the six-byte load of the C4 flag. Preserve all original
// registers/flags except AL, which is exactly the original instruction output.
// Original test/jump and subsequent distance/target checks still execute.
static __declspec(naked) void riskC4Entry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push esi
 push 4
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,1
 push 51A4DEh
 ret
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,byte ptr [ecx+0EC2h]
 push 51A4DEh
 ret
}}
static __declspec(naked) void riskC4MovementEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push ebp
 push 4
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,1
 push 51C331h
 ret
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,byte ptr [ecx+0EC2h]
 push 51C331h
 ret
}}
// Native sidebar CanBuild call ABI: ECX=house; stack=type,limitOnly,includeProduction.
// Only the UI callsites change limitOnly. The simulation function stays intact.
static __declspec(naked) void riskTechEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 mov eax,[ebx+12]
 push ecx
 push dword ptr [eax+8]
 push 5
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 mov eax,[ebx+12]
 mov dword ptr [eax+12],1
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 4F7870h
 ret
}}
// The capture shows a shared control predicate, not selection-only calls.
// Installed only as one manager-owned group after context/heartbeat preflight.
// These hot leaf functions do no polling, lease acquisition, allocation or IO.
static __declspec(naked) void riskControlEntry(){__asm {
 mov eax,1
 ret
}}
static __declspec(naked) void riskControlOwnerEntry(){__asm {
 mov edx,dword ptr ds:[0A83D4Ch]
 xor eax,eax
 cmp ecx,edx
 sete al
 ret
}}
struct RiskSite {uint32_t address;unsigned size;uint8_t before[16],after[16];};
static RiskSite riskSites[]={
 {experimentalpatch::Placement,5,{0xA1,0x4C,0x3D,0xA8,0},{}},
 {experimentalpatch::AirType,4,{0x50,0xCB,0x41,0},{}},
 {experimentalpatch::AirArrival,10,{0xC7,0x86,0xBC,0,0,0,3,0,0,0},{}},
 {experimentalpatch::ShroudDraw,5,{0x8B,0x45,0x10,0x53,0x56},{}},
 {experimentalpatch::FogDraw,5,{0x8B,0x44,0x24,0x38,0x53},{}},
 {experimentalpatch::C4Execution,6,{0x8A,0x81,0xC2,0x0E,0,0},{}},
 {experimentalpatch::TechDraw,5,{0xE8,0x99,0xE0,0xE4,0xFF},{}},
 {experimentalpatch::TechPrune,5,{0xE8,0xEA,0xD0,0xE4,0xFF},{}},
 {experimentalpatch::C4Movement,6,{0x8A,0x81,0xC2,0x0E,0,0},{}}
};
static void prepareRiskSites(){static bool ready=false;if(ready)return;ready=true;
 const uint32_t targets[]={uint32_t(uintptr_t(&riskPlacementEntry)),uint32_t(uintptr_t(&riskAirTypeEntry)),uint32_t(uintptr_t(&riskAirArrivalEntry)),uint32_t(uintptr_t(&riskShroudEntry)),uint32_t(uintptr_t(&riskFogEntry)),uint32_t(uintptr_t(&riskC4Entry)),uint32_t(uintptr_t(&riskTechEntry)),uint32_t(uintptr_t(&riskTechEntry)),uint32_t(uintptr_t(&riskC4MovementEntry))};
 for(unsigned i=0;i<sizeof(riskSites)/sizeof(riskSites[0]);++i){auto&s=riskSites[i];
  if(s.size==4)memcpy(s.after,&targets[i],4);
  else{memset(s.after,0x90,s.size);s.after[0]=experimentalpatch::callSite(s.address)?0xE8:0xE9;uint32_t offset=targets[i]-s.address-5;memcpy(s.after+1,&offset,4);}
 }
}
static bool riskSiteCurrent(const RiskSite&s){uint8_t bytes[16]{};return read(s.address,bytes,s.size)&&!memcmp(bytes,s.after,s.size);}
static bool restoreRiskSite(RiskSite&s){
 if(!codeHookRegistry.active(s.address))return true;
 return gameentry::patch(gameentry::Client::Assist,s.address,s.after,s.before,s.size);
}
static bool installRiskSite(RiskSite&s){
 // Recheck on ordinary feature maintenance, never on each drawn cell. If a
 // later platform patch changes an ABI dependency, release only our own site.
 if(!patchcontext::compatible(s.address,[](uint32_t at,void*out,size_t n){return read(at,out,n);})){restoreRiskSite(s);return false;}
 if(codeHookRegistry.active(s.address)){if(riskSiteCurrent(s))return !codeHookRegistry.conflict(s.address);codeHookRegistry.markConflict(s.address);return false;}
 return gameentry::patch(gameentry::Client::Assist,s.address,s.before,s.after,s.size);
}
#include "master_tech_bridge.h"
static bool setTechSites(bool enabled){
 prepareRiskSites();prepareMasterTech();riskTechActive=false;
 if(enabled&&techImplementation==2){
  // The old call gates and master prune have adjacent ABI dependencies.
  // Restore the old pair before preflighting the complete new group.
  bool a=restoreRiskSite(riskSites[6]),b=restoreRiskSite(riskSites[7]);bool ok=a&&b;
  for(auto&s:masterTechSites){uint8_t bytes[16]{};bool active=codeHookRegistry.active(s.address);
   if(!patchcontext::compatible(s.address,[](uint32_t p,void*out,size_t n){return read(p,out,n);})||codeHookRegistry.conflict(s.address)||!read(s.address,bytes,s.size)||memcmp(bytes,active?s.after:s.before,s.size))ok=false;
  }
  if(ok)for(auto&s:masterTechSites)if(!installRiskSite(s)){ok=false;break;}
  if(ok){riskTechActive=true;return true;}
  restoreMasterTech();return false;
 }
 bool masterOff=restoreMasterTech();
 if(enabled&&masterOff){riskTechActive=installRiskSite(riskSites[6])&&installRiskSite(riskSites[7]);if(riskTechActive)return true;}
 bool a=restoreRiskSite(riskSites[6]),b=restoreRiskSite(riskSites[7]);return !enabled&&masterOff&&a&&b;
}
// Preserve both owner-index paths before enabling the common predicate.
// 0x637E40 is a verified CALL boundary. Never write the sample's 0x6378E0.
static RiskSite controlSites[]={
 {experimentalpatch::ControlOwner,5,{0xE8,0x31,0x7D,0xED,0xFF},{}},
 {experimentalpatch::ControlPlanning,5,{0xE8,0xAB,0x38,0xED,0xFF},{}},
 {experimentalpatch::ControlHouse,5,{0xA1,0x38,0xB2,0xA8,0},{}}
};
static void controlSitesTick(bool enabled){
 enabled=enabled&&playertest::Available;
 if(riskControlActive&&!enabled&&riskContextReady())unselect();
 riskControlActive=false;bool ok=true;
 static bool prepared=false;
 if(!prepared){
  for(auto&s:controlSites){const uint32_t target=uint32_t(uintptr_t(s.address==experimentalpatch::ControlHouse?&riskControlEntry:&riskControlOwnerEntry));
   s.after[0]=experimentalpatch::callSite(s.address)?0xE8:0xE9;const uint32_t offset=target-s.address-5;memcpy(s.after+1,&offset,4);}
  prepared=true;
 }
 if(enabled){
  auto&s=controlSites[2];uint8_t original[5]{};
  if(!codeHookRegistry.active(s.address)){
   if(!read(s.address,original,5)||!hookregistry::knownOriginal(s.address,original,5))ok=false;
   else memcpy(s.before,original,5);
  }
  if(ok)for(auto&site:controlSites)if(!installRiskSite(site)){ok=false;break;}
 }
 // Restore the common predicate first; preserve foreign bytes on conflict.
 if(!enabled||!ok)for(int i=2;i>=0;--i)if(!restoreRiskSite(controlSites[i]))ok=false;
 riskControlActive=enabled&&ok;if(sh)sh->playerControlStatus=ok?(enabled?1:0):-2;
}
static bool redrawRevealTest();
static void restoreSmallMapCamera(),restoreHighScroll();
static void restorePlacement(){
 restoreSmallMapCamera();restoreHighScroll();
 const bool wasRendered=riskRenderActive;
 riskPlacementActive=riskAirActive=riskRenderActive=riskC4Active=false;prepareRiskSites();
 controlSitesTick(false);
 bool place=restoreRiskSite(riskSites[0]);
 bool type=restoreRiskSite(riskSites[1]),arrival=restoreRiskSite(riskSites[2]);
 snap.placementStatus=place?0:-2;snap.nativeAirStatus=type&&arrival?0:-2;
 bool shroud=restoreRiskSite(riskSites[3]),fog=restoreRiskSite(riskSites[4]);
 snap.renderRevealStatus=shroud&&fog?0:-2;
 bool c4Arrival=restoreRiskSite(riskSites[5]),c4Movement=restoreRiskSite(riskSites[8]);
 snap.c4TestStatus=c4Arrival&&c4Movement?0:-2;
 // Stop/host exit still needs one native redraw after restoring the leaves.
 // A destroyed, changed, paused or faulted world must never be called here.
 riskRenderRedrawPending=riskRenderRedrawPending||wasRendered;
 if(riskRenderRedrawPending&&redrawRevealTest())riskRenderRedrawPending=false;
}
static bool redrawRevealTest(){
 // Native redraw bookkeeping only, once per toggle/recovery; never reveal/register cells.
 static const uint8_t signature[]={0x83,0xEC,0x58,0x8B,0x0D,0x88,0xCE,0xB0,0};uint8_t actual[sizeof signature]{};
 const uint32_t tactical=get<uint32_t>(0x887324);
 struct Area {int x,y,w,h;};Area area{};
 if(tactical<0x10000||!riskContextReady()||!read(0x6D2790,actual,sizeof actual)||memcmp(actual,signature,sizeof actual)||!read(0x886FA0,&area,sizeof area)||area.w<=0||area.h<=0||area.w>16384||area.h>16384)return false;
 call<void>(0x6D2790,tactical,area,false);return true;
}
static void placementTick(){
 prepareRiskSites();
 static uint32_t priorStrategy=0;
 bool priorRender=riskRenderActive;controlSitesTick(sh->controlAll!=0);
 // Infantry entry is now an ordinary planning event. Never install either
 // capability override, including when an older configuration enables C4Test.
 riskC4Active=false;
 {bool a=restoreRiskSite(riskSites[5]),b=restoreRiskSite(riskSites[8]);snap.c4TestStatus=!a||!b?-2:sh->c4Test?1:0;}
 if(sh->renderReveal){
  riskRenderActive=installRiskSite(riskSites[3])&&installRiskSite(riskSites[4]);
  if(!riskRenderActive){restoreRiskSite(riskSites[3]);restoreRiskSite(riskSites[4]);}
  snap.renderRevealStatus=riskRenderActive?1:-2;
 }else{riskRenderActive=false;bool a=restoreRiskSite(riskSites[3]),b=restoreRiskSite(riskSites[4]);snap.renderRevealStatus=a&&b?0:-2;}
 if(priorRender!=riskRenderActive||priorStrategy!=sh->renderReveal)riskRenderRedrawPending=true;
 priorStrategy=sh->renderReveal;
 if(riskRenderRedrawPending&&redrawRevealTest())riskRenderRedrawPending=false;
 if(sh->flags&BuildAnywhere){riskPlacementActive=installRiskSite(riskSites[0]);snap.placementStatus=riskPlacementActive?1:-2;}
 else{riskPlacementActive=false;snap.placementStatus=restoreRiskSite(riskSites[0])?0:-2;}
 if(airpathpolicy::native(get<int>(0xA8B238),(sh->flags&NativeAirWaypoints)!=0)){
  static const uint8_t nextSignature[]={0x81,0xEC,0xE0,0,0,0,0x53,0x56,0x8B,0xF1};uint8_t actual[sizeof nextSignature]{};
  bool supported=read(0x6385C0,actual,sizeof actual)&&!memcmp(actual,nextSignature,sizeof actual);
  // Behavior first, frontend last. Partial installation rolls back as a group.
  riskAirActive=supported&&installRiskSite(riskSites[2])&&installRiskSite(riskSites[1]);
  if(!riskAirActive){restoreRiskSite(riskSites[1]);restoreRiskSite(riskSites[2]);}
  snap.nativeAirStatus=riskAirActive?1:supported?-2:-1;
 }else{riskAirActive=false;bool a=restoreRiskSite(riskSites[1]),b=restoreRiskSite(riskSites[2]);
  snap.nativeAirStatus=a&&b?((sh->flags&NativeAirWaypoints)&&airpathpolicy::events(get<int>(0xA8B238),false,true)?2:0):-2;}
}
