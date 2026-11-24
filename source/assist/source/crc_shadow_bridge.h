#include "../../shared/source/debug_entry_checks.h"
#pragma once
#include "crc_shadow_policy.h"
#include "crc_shadow_single_store_policy.h"
extern "C" unsigned __cdecl GuardedMapCrcRevealStore(unsigned,unsigned);
extern "C" {
volatile unsigned char crcMapSkipStore=0;
}
static crcshadowsingle::Scope crcMapSingleScope;
static bool crcMapActive,crcMapInstalled,crcMapFaulted;
static uint32_t crcMapScenario,crcMapItems,crcMapLocal,crcMapLocalID,crcMapEpoch,crcMapUnsafeEpoch;
static int crcMapMode,crcMapState,crcMapTargetIndex=32;
static uint32_t crcMapSuppressed,crcMapFaults;
struct CrcMapSite {crcshadow::Site spec;uint8_t patch[7];};
static CrcMapSite crcMapSite;

static bool crcMapWorld(){
 uint32_t scenario=0,items=0,local=0,id=0;int mode=-1;
 return crcMapEpoch&&crcMapEpoch==match&&read(0xA8B230,&scenario,4)&&scenario==crcMapScenario&&
  read(0xA8022C,&items,4)&&items==crcMapItems&&read(0xA83D4C,&local,4)&&local==crcMapLocal&&
  read(local+0x10,&id,4)&&id==crcMapLocalID&&read(0xA8B238,&mode,4)&&mode==crcMapMode;
}
static bool crcMapIdentity(uint32_t p,crcshadow::Identity&i){
 uint32_t id=0;int index=-1;
 if(p<0x10000||!read(p+0x10,&id,4)||!id||!read(p+0x30,&index,4))return false;
 Vec houses=vec(0xA80228,32);
 if(index<0||index>=houses.count||houses.items!=crcMapItems||item(houses,index)!=p)return false;
 i={crcMapEpoch,p,id,unsigned(index)};return true;
}
static void crcMapPublish(){
 if(!sh)return;
 sh->crcMapShadowStatus=crcMapState;
 sh->crcMapShadowTracked=crcMapInstalled?1u:0u;
 sh->crcMapShadowSubstitutions=crcMapSuppressed;
 sh->crcMapShadowFaults=crcMapFaults;
}
static void crcMapResetWorld(){
 crcMapScenario=crcMapItems=crcMapLocal=crcMapLocalID=crcMapEpoch=0;
 crcMapMode=0;crcMapTargetIndex=32;
}
static void crcMapFault(bool publish=true){
 if(!crcMapFaulted)++crcMapFaults;
 crcMapFaulted=true;crcMapActive=false;crcMapUnsafeEpoch=crcMapEpoch;crcMapState=-3;
 if(crcMapSingleScope.active)crcMapSingleScope.faulted=true;
 if(publish)crcMapPublish();
}
extern "C" unsigned __cdecl AssistMapCrcRevealStore(unsigned house,unsigned savedEBX){
 if(!crcMapActive||crcMapFaulted||!crcMapSingleScope.active||house!=crcMapSingleScope.house)return savedEBX;
 if(!crcMapWorld()){crcMapFault(false);return savedEBX;}
 crcshadow::Identity id;
 if(!crcMapIdentity(house,id)||id.index!=unsigned(crcMapTargetIndex)){crcMapFault(false);return savedEBX;}
 const auto decision=crcshadowsingle::beforeStore(crcMapSingleScope,id.epoch,id.pointer,id.id);
 if(decision==crcshadowsingle::Result::SuppressStore){
  crcMapSkipStore=1;++crcMapSuppressed;
 }else if(decision==crcshadowsingle::Result::IdentityChanged)crcMapFault(false);
 return savedEBX;
}
extern "C" void __cdecl AssistMapCrcRevealAbort(unsigned house){
 if(crcshadowsingle::abort(crcMapSingleScope,house))crcMapSkipStore=1;
 crcMapFault(false);
}

// Preserve the original machine state around the guarded callback. Only the
// exact MapIsClear MOV is conditional; all later Map::Reveal code is native.
static __declspec(naked) void crcMapRevealSingleEntry(){
 __asm { pushfd } __asm { pushad } __asm { mov ebx,esp }
 __asm { sub esp,528 } __asm { and esp,-16 } __asm { fxsave [esp] }
 __asm { cld } __asm { fninit } __asm { fldcw [esp] }
 __asm { mov dword ptr [esp+512],1F80h } __asm { ldmxcsr [esp+512] }
 __asm { push dword ptr [ebx+16] } __asm { push dword ptr [ebx+24] }
 __asm { call GuardedMapCrcRevealStore } __asm { add esp,8 }
 __asm { mov dword ptr [ebx+16],eax } __asm { fxrstor [esp] }
 __asm { mov esp,ebx } __asm { popad } __asm { popfd }
 __asm {
  pushfd
  cmp byte ptr [crcMapSkipStore],0
  jne skip_original_store
  mov byte ptr [ecx+241h],bl
 skip_original_store:
  mov byte ptr [crcMapSkipStore],0
  popfd
  push 577DB6h
  ret
 }
}
static void crcMapPrepare(){
 static bool prepared=false;if(prepared)return;prepared=true;
 crcMapSite.spec=crcshadow::Sites[6];
 memset(crcMapSite.patch,0x90,crcMapSite.spec.size);
 crcMapSite.patch[0]=0xe9;
 const uint32_t rel=uint32_t(uintptr_t(&crcMapRevealSingleEntry))-crcMapSite.spec.address-5;
 memcpy(crcMapSite.patch+1,&rel,4);
}
static bool restoreCrcMapShadow(){
 if(crcMapSingleScope.active){crcMapState=-2;crcMapPublish();return false;}
 crcMapActive=false;crcMapPrepare();crcMapSkipStore=0;
 const auto&s=crcMapSite.spec;
 if(codeHookRegistry.conflict(s.address)){crcMapState=-2;crcMapPublish();return false;}
 if(codeHookRegistry.active(s.address)&&
    !gameentry::patch(gameentry::Client::Assist,s.address,crcMapSite.patch,s.original,s.size)){
  crcMapInstalled=true;crcMapState=-2;crcMapPublish();return false;
 }
 if(codeHookRegistry.active(s.address)){crcMapInstalled=true;crcMapState=-2;crcMapPublish();return false;}
 crcMapInstalled=false;crcMapResetWorld();crcMapState=0;crcMapPublish();return true;
}
static void crcMapShadowTick(){
 if(crcMapFaulted&&!crcMapInstalled&&crcMapUnsafeEpoch!=match){crcMapFaulted=false;crcMapUnsafeEpoch=0;}
 const bool requested=sh&&sh->crcMapShadow&&!sh->stop&&!fault;
 if(!requested){
  if(crcMapInstalled||crcMapActive||crcMapState!=0||crcMapSingleScope.active)restoreCrcMapShadow();
  return;
 }
 if(crcMapFaulted||crcMapUnsafeEpoch==match){
  if(crcMapInstalled||crcMapActive){if(!restoreCrcMapShadow())return;}
  else crcMapResetWorld();
  if(crcMapState!=-4){crcMapState=-4;crcMapPublish();}return;
 }
 if(!(sh->flags&ExperimentalReveal)||sh->renderReveal||!eventdelivery::isMultiplayer(get<int>(0xA8B238))){
  if(crcMapInstalled||crcMapActive){if(!restoreCrcMapShadow())return;}
  else crcMapResetWorld();
  if(crcMapState!=-5){crcMapState=-5;crcMapPublish();}return;
 }
 if(GetModuleHandleW(L"Ares.dll")||GetModuleHandleW(L"Phobos.dll")){
  if(crcMapInstalled||crcMapActive){if(!restoreCrcMapShadow())return;}
  else crcMapResetWorld();
  if(crcMapState!=-6){crcMapState=-6;crcMapPublish();}return;
 }
 // No game data is changed by this adapter, so a retired/unreadable world can
 // safely drop the single code patch and capture a fresh identity.
 if(crcMapInstalled&&!crcMapWorld()){
  if(!restoreCrcMapShadow())return;
  crcMapFaulted=false;crcMapUnsafeEpoch=0;
 }
 if(!crcMapInstalled){
  if(!match||!lastHouse||snap.status!=1||!get<bool>(0xA8E9A0)){if(crcMapState!=2){crcMapState=2;crcMapPublish();}return;}
  crcMapPrepare();const auto&s=crcMapSite.spec;uint8_t actual[7]{};
  if(codeHookRegistry.active(s.address)||codeHookRegistry.conflict(s.address)||
     !read(s.address,actual,s.size)||!hookregistry::admissibleOriginal(s.address,actual,s.size)){
   crcMapState=-2;crcMapPublish();return;
  }
  crcMapEpoch=match;crcMapScenario=get<uint32_t>(0xA8B230);crcMapItems=get<uint32_t>(0xA8022C);
  crcMapLocal=get<uint32_t>(0xA83D4C);crcMapLocalID=crcMapLocal?get<uint32_t>(crcMapLocal+0x10):0;
  crcMapMode=get<int>(0xA8B238);
  if(!crcMapScenario||crcMapItems<0x10000||crcMapLocal<0x10000||!crcMapLocalID){
   crcMapResetWorld();crcMapState=2;crcMapPublish();return;
  }
  const bool ok=gameentry::patch(gameentry::Client::Assist,s.address,s.original,crcMapSite.patch,s.size);
  crcMapInstalled=codeHookRegistry.active(s.address);
  if(!ok||!crcMapInstalled){
   if(crcMapInstalled)restoreCrcMapShadow();
   crcMapState=-2;crcMapPublish();return;
  }
 }
 if(!crcMapActive||crcMapState!=1){crcMapActive=true;crcMapState=1;crcMapPublish();}
}
static bool crcMapToolBegin(uint32_t house){
 if(!sh||!sh->crcMapShadow)return !crcMapSingleScope.active&&!crcMapSkipStore;
 if(!crcMapActive||!crcMapInstalled||crcMapFaulted||crcMapSingleScope.active||crcMapSkipStore||!crcMapWorld())return false;
 crcMapPrepare();uint8_t actual[7]{};
 const auto&site=crcMapSite.spec;
 if(!codeHookRegistry.active(site.address)||!read(site.address,actual,site.size)||memcmp(actual,crcMapSite.patch,site.size)!=0){
  crcMapFault();return false;
 }
 crcshadow::Identity id;
 if(!crcMapIdentity(house,id)){crcMapFault();return false;}
 const auto result=crcshadowsingle::begin(crcMapSingleScope,id.epoch,id.pointer,id.id);
 if(result!=crcshadowsingle::Result::Armed)return false;
 crcMapTargetIndex=int(id.index);return true;
}
static void crcMapToolEnd(bool completed){
 if(crcMapSingleScope.active&&(!completed||crcMapSingleScope.faulted))crcMapFault(false);
 crcshadowsingle::end(crcMapSingleScope);
 crcMapTargetIndex=32;crcMapSkipStore=0;
 if(!crcMapFaulted)crcMapPublish();
}
static void crcMapShadowRewind(uint32_t nextEpoch){
 if(crcMapInstalled||crcMapActive||crcMapSingleScope.active){
  crcMapFault(false);crcMapUnsafeEpoch=nextEpoch;
 }
}
