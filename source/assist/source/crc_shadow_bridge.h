#pragma once
#include "crc_shadow_policy.h"
extern "C" unsigned __cdecl GuardedMapCrcShadow(unsigned,unsigned,unsigned);
static crcshadow::Model crcMapModel;
static bool crcMapActive,crcMapInstalled,crcMapFaulted,crcMapToolScope,crcMapNativeReconstructed;
static uint32_t crcMapScenario,crcMapItems,crcMapLocal,crcMapLocalID,crcMapEpoch,crcMapUnsafeEpoch;
static int crcMapMode,crcMapLastFrame=-1,crcMapState;
static uint32_t crcMapSubstitutions,crcMapFaults;
struct CrcMapSite {crcshadow::Site spec;uint8_t patch[7];};
static CrcMapSite crcMapSites[crcshadow::SiteCount];
static bool crcMapWorld(){
 uint32_t scenario=0,items=0,local=0,id=0;int mode=-1;
 return crcMapEpoch&&crcMapEpoch==match&&read(0xA8B230,&scenario,4)&&scenario==crcMapScenario&&
  read(0xA8022C,&items,4)&&items==crcMapItems&&read(0xA83D4C,&local,4)&&local==crcMapLocal&&
  read(local+0x10,&id,4)&&id==crcMapLocalID&&read(0xA8B238,&mode,4)&&mode==crcMapMode;
}
static bool crcMapIdentity(uint32_t p,crcshadow::Identity&i){
 uint32_t id=0;int index=-1;if(p<0x10000||!read(p+0x10,&id,4)||!id||!read(p+0x30,&index,4))return false;
 Vec houses=vec(0xA80228,32);if(index<0||index>=houses.count||houses.items!=crcMapItems||item(houses,index)!=p)return false;
 i={crcMapEpoch,p,id,unsigned(index)};return true;
}
static bool crcMapPointerTracked(uint32_t p){for(const auto&e:crcMapModel.entries())if(e.active&&e.identity.pointer==p)return true;return false;}
static void crcMapUnknown(){crcMapFaulted=true;crcMapUnsafeEpoch=crcMapEpoch;crcMapState=-3;++crcMapFaults;}
extern "C" void __cdecl AssistMapCrcAbort(){
 // SEH cleanup: DLL state only. Never dereference possibly stale Shared/world.
 crcMapFaulted=true;crcMapToolScope=false;crcMapUnsafeEpoch=crcMapEpoch;crcMapState=-3;++crcMapFaults;
}
extern "C" unsigned __cdecl AssistMapCrcShadow(unsigned op,unsigned house,unsigned savedEBX){
 if(op==1){crcMapModel.invalidatePointer(house);if(crcMapInstalled&&house==crcMapLocal)crcMapNativeReconstructed=true;return savedEBX;} // constructor, identity not ready yet
 if(!crcMapActive||crcMapFaulted)return savedEBX;
 if(!crcMapWorld()){crcMapUnknown();return savedEBX;}
 const int frame=get<int>(0xA8ED84);if(frame<0||frame<crcMapLastFrame){if(!crcMapNativeReconstructed)crcMapUnknown();return savedEBX;}
 crcMapLastFrame=frame;crcMapModel.frame(crcMapEpoch,frame);
 crcshadow::Identity id;if(!crcMapIdentity(house,id)){if(crcMapPointerTracked(house))crcMapUnknown();return savedEBX;}
 if(op==0){
  const auto*e=crcMapModel.find(id);if(!e||!e->active)return savedEBX;
  const uint8_t live=uint8_t(savedEBX);
  if(!e->known||e->blocked||live>1||live!=e->display){crcMapModel.unknownWrite(id);crcMapUnknown();return savedEBX;}
  const uint8_t original=crcMapModel.crcValue(id,live);if(original!=live)++crcMapSubstitutions;
  return (savedEBX&0xffffff00u)|original;
 }
 const uint8_t value=(op==2||op==7)?1:(op==4||op==5)?0:uint8_t(savedEBX);
 if(value>1){crcMapModel.unknownWrite(id);crcMapUnknown();return savedEBX;}
 if(crcMapToolScope){
  if(op!=6||!crcMapModel.beginTool(id,get<uint8_t>(house+0x241),value))crcMapUnknown();
 }else crcMapModel.naturalWrite(id,value);
 return savedEBX;
}
// Every native leaf preserves the incoming FP/GPR/flags. CRC first performs
// its original BL load, so any disabled/faulted callback returns the real byte.
#define CRC_MAP_CALLBACK(OP,PTR) \
 __asm { pushfd } __asm { pushad } __asm { mov ebx,esp } \
 __asm { sub esp,528 } __asm { and esp,-16 } __asm { fxsave [esp] } \
 __asm { cld } __asm { fninit } __asm { fldcw [esp] } \
 __asm { mov dword ptr [esp+512],1F80h } __asm { ldmxcsr [esp+512] } \
 __asm { push dword ptr [ebx+16] } __asm { push dword ptr [ebx+PTR] } \
 __asm { push OP } __asm { call GuardedMapCrcShadow } __asm { add esp,12 } \
 __asm { mov dword ptr [ebx+16],eax } __asm { fxrstor [esp] } \
 __asm { mov esp,ebx } __asm { popad } __asm { popfd }
static __declspec(naked) void crcMapReadEntry(){
 __asm { mov bl,byte ptr [ecx+241h] }
 CRC_MAP_CALLBACK(0,24)
 __asm { push 64DCD0h } __asm { ret }
}
static __declspec(naked) void crcMapInitEntry(){CRC_MAP_CALLBACK(1,8)
 __asm { mov byte ptr [ebp+241h],bl } __asm { push 4F5771h } __asm { ret }
}
static __declspec(naked) void crcMapDefeatEntry(){CRC_MAP_CALLBACK(2,24)
 __asm { mov byte ptr [ecx+241h],1 } __asm { push 4FC32Fh } __asm { ret }
}
static __declspec(naked) void crcMapShroudEntry(){CRC_MAP_CALLBACK(3,24)
 __asm { mov byte ptr [ecx+241h],bl } __asm { push 577AD0h } __asm { ret }
}
static __declspec(naked) void crcMapResetEntry(){CRC_MAP_CALLBACK(4,24)
 __asm { mov byte ptr [ecx+241h],0 } __asm { push 577BCEh } __asm { ret }
}
static __declspec(naked) void crcMapResetTEntry(){CRC_MAP_CALLBACK(5,24)
 __asm { mov byte ptr [ecx+241h],0 } __asm { push 577CAEh } __asm { ret }
}
static __declspec(naked) void crcMapRevealEntry(){CRC_MAP_CALLBACK(6,24)
 __asm { mov byte ptr [ecx+241h],bl } __asm { push 577DB6h } __asm { ret }
}
static __declspec(naked) void crcMapRevealAllEntry(){CRC_MAP_CALLBACK(7,24)
 __asm { mov byte ptr [ecx+241h],1 } __asm { push 577F4Fh } __asm { ret }
}
#undef CRC_MAP_CALLBACK
static void crcMapPrepare(){static bool prepared=false;if(prepared)return;prepared=true;
 const uint32_t targets[]={uint32_t(uintptr_t(&crcMapReadEntry)),uint32_t(uintptr_t(&crcMapInitEntry)),
  uint32_t(uintptr_t(&crcMapDefeatEntry)),uint32_t(uintptr_t(&crcMapShroudEntry)),uint32_t(uintptr_t(&crcMapResetEntry)),
  uint32_t(uintptr_t(&crcMapResetTEntry)),uint32_t(uintptr_t(&crcMapRevealEntry)),uint32_t(uintptr_t(&crcMapRevealAllEntry))};
 for(unsigned n=0;n<crcshadow::SiteCount;++n){auto&s=crcMapSites[n];s.spec=crcshadow::Sites[n];memset(s.patch,0x90,s.spec.size);s.patch[0]=0xe9;
  const uint32_t rel=targets[n]-s.spec.address-5;memcpy(s.patch+1,&rel,4);}
}
static void crcMapPublish(){if(!sh)return;unsigned tracked=0;for(const auto&e:crcMapModel.entries())if(e.active&&e.known&&!e.blocked)++tracked;
 sh->crcMapShadowStatus=crcMapState;sh->crcMapShadowTracked=tracked;sh->crcMapShadowSubstitutions=crcMapSubstitutions;sh->crcMapShadowFaults=crcMapFaults;
}
static bool restoreCrcMapShadow(){
 crcMapPrepare();crcMapToolScope=false;
 if(crcMapWorld()&&get<int>(0xA8ED84)>=crcMapLastFrame)for(const auto&e:crcMapModel.entries())if(e.active&&e.known&&!e.blocked){
  crcshadow::Identity current;if(!crcMapIdentity(e.identity.pointer,current)||!(current==e.identity))continue;
  uint8_t live=0;if(!read(current.pointer+0x241,&live,1))return false;
  if(live!=e.display){crcMapUnsafeEpoch=crcMapEpoch;continue;} // unknown origin: never overwrite it
  if(live!=e.original){if(!gameentry::active(gameentry::Client::Assist))return false;
   put<uint8_t>(current.pointer+0x241,e.original);if(get<uint8_t>(current.pointer+0x241)!=e.original)return false;
   crcMapModel.naturalWrite(current,e.original);}
 }
 crcMapActive=false;bool ok=true;for(int n=int(crcshadow::SiteCount)-1;n>=0;--n){auto&s=crcMapSites[n];
  if(codeHookRegistry.active(s.spec.address)&&!gameentry::patch(gameentry::Client::Assist,s.spec.address,s.patch,s.spec.original,s.spec.size))ok=false;}
 if(!ok){crcMapInstalled=true;crcMapState=-2;crcMapPublish();return false;}
 crcMapInstalled=false;crcMapModel.clear();crcMapEpoch=0;crcMapLastFrame=-1;crcMapState=0;crcMapPublish();return true;
}
static void crcMapShadowTick(){
 crcMapPrepare();if(crcMapFaulted&&!crcMapInstalled&&crcMapUnsafeEpoch!=match)crcMapFaulted=false;
 if(sh&&sh->renderReveal==3&&match)crcMapUnsafeEpoch=match; // direct mode has no trusted source tag
 const bool requested=sh&&sh->crcMapShadow&&!sh->stop&&!fault;
 if(!requested){restoreCrcMapShadow();return;}
 if(crcMapFaulted||crcMapUnsafeEpoch==match){if(!restoreCrcMapShadow())return;crcMapState=-4;crcMapPublish();return;}
 if(!(sh->flags&ExperimentalReveal)||sh->renderReveal||!eventdelivery::isMultiplayer(get<int>(0xA8B238))){
  if(!restoreCrcMapShadow())return;crcMapState=-5;crcMapPublish();return;}
 if(GetModuleHandleW(L"Ares.dll")||GetModuleHandleW(L"Phobos.dll")){if(!restoreCrcMapShadow())return;crcMapState=-6;crcMapPublish();return;}
 if(crcMapInstalled&&!crcMapWorld()){if(!restoreCrcMapShadow())return;crcMapFaulted=false;}
 if(!crcMapInstalled){
  if(!match||!lastHouse||snap.status!=1||!get<bool>(0xA8E9A0)){crcMapState=2;crcMapPublish();return;}
  for(const auto&s:crcMapSites){uint8_t actual[7]{};if(codeHookRegistry.active(s.spec.address)||codeHookRegistry.conflict(s.spec.address)||
   !read(s.spec.address,actual,s.spec.size)||memcmp(actual,s.spec.original,s.spec.size)){crcMapState=-2;crcMapPublish();return;}}
  crcMapEpoch=match;crcMapScenario=get<uint32_t>(0xA8B230);crcMapItems=get<uint32_t>(0xA8022C);
  crcMapLocal=get<uint32_t>(0xA83D4C);crcMapLocalID=get<uint32_t>(crcMapLocal+0x10);crcMapMode=get<int>(0xA8B238);crcMapNativeReconstructed=false;
  crcMapModel.clear();crcMapModel.frame(match,get<int>(0xA8ED84));crcMapLastFrame=get<int>(0xA8ED84);crcMapFaulted=false;
  bool ok=true;for(auto&s:crcMapSites)if(!gameentry::patch(gameentry::Client::Assist,s.spec.address,s.spec.original,s.patch,s.spec.size)){ok=false;break;}
  crcMapInstalled=true;if(!ok){restoreCrcMapShadow();crcMapState=-2;crcMapPublish();return;}
 }
 crcMapActive=true;crcMapState=1;crcMapPublish();
}
static bool crcMapToolBegin(uint32_t house){
 if(!sh||!sh->crcMapShadow){crcMapUnsafeEpoch=match;return true;}
 if(!crcMapActive||crcMapFaulted||crcMapToolScope||!crcMapWorld())return false;
 crcshadow::Identity id;uint8_t before=0;
 if(!crcMapIdentity(house,id)||!read(house+0x241,&before,1)||!crcMapModel.beginTool(id,before,1)){crcMapUnknown();crcMapPublish();return false;}
 crcMapToolScope=true;return true;
}
static void crcMapToolEnd(){crcMapToolScope=false;crcMapPublish();}

static void crcMapShadowRewind(uint32_t nextEpoch){
 // A rollback in the same live identity may be a load/restore. An observed
 // House construction permits a fresh identity; otherwise never overwrite
 // new data using pre-load shadow state.
 if(crcMapInstalled&&crcMapWorld()&&!crcMapNativeReconstructed){crcMapUnsafeEpoch=nextEpoch;crcMapFaulted=true;}
}
