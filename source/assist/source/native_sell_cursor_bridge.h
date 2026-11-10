#include "../../shared/source/debug_entry_checks.h"
#include "../../shared/source/native_sell_policy.h"
// Keep native CanSell and ordinary queued Sell. SP-AI uses the complete native
// event receiver and skips the downstream constructor/OutList append.
static bool networkHousePreserveReady();
static bool nativeSellCursorActive;
struct NativeSellCandidate {
 uint32_t object=0,id=0,owner=0,ownerID=0,index=0,epoch=0,local=0,token=0,tick=0;
 int frame=0,session=0;
};
static NativeSellCandidate nativeSellCandidate;
static bool nativeSellQueueCompatible(){
 return nativesell::queueCompatible([](uint32_t at,void*out,size_t n){return read(at,out,n);});
}
static bool nativeSellCursorReady(){
 if(!nativeSellCursorActive||!sh||!sh->nativeSellCursor||!sh->foreground||
    snap.status!=1||snap.match!=match||!riskReady(true)||get<bool>(0xA8BC60+25)||
    !get<bool>(0x880999)||!playableHouse(lastHouse))return false;
 return !eventdelivery::isMultiplayer(get<int>(0xA8B238))||(networkHousePreserveReady()&&nativeSellQueueCompatible());
}
static bool nativeSellCursorTarget(uint32_t object,uint32_t& owner,uint32_t& index){
 if(!object||!worldObjects.contains(object)||!live(object)||kind(object)!=3||
    !nativeEnemy(object,lastHouse))return false;
 const uint32_t id=get<uint32_t>(object+0x10);owner=get<uint32_t>(object+0x21C);
 if(!id||worldByID(id)!=object||!playableHouse(owner))return false;
 const int ownerIndex=get<int>(owner+0x30);Vec houses=vec(0xA80228,32);
 if(ownerIndex<0||ownerIndex>=houses.count||item(houses,ownerIndex)!=owner)return false;
 const int session=get<int>(0xA8B238);
 if(eventdelivery::isMultiplayer(session)&&!networkSenderHouse(ownerIndex))return false;
 if(nativesell::requiresNativeQueue(eventdelivery::isSingleplayer(session),sellHumanHouse(owner))&&!nativeSellQueueCompatible())return false;
 index=uint32_t(ownerIndex);return true;
}
static int nativeSellCursorAction(unsigned op,uint32_t object,uint32_t suppliedOwner){
 if(op!=9&&op!=10)return 0;
 if(!nativeSellCursorReady()){nativeSellCandidate={};return 0;}
 // Cursor callbacks can run without a toolbox command or another feature that
 // requests the world index. Refresh on this guarded game-thread entry as well.
 indexWorld(get<int>(0xA8ED84));
 uint32_t owner=0,index=0;
 if(!nativeSellCursorTarget(object,owner,index)){nativeSellCandidate={};return 0;}
 const uint32_t id=get<uint32_t>(object+0x10),ownerID=get<uint32_t>(owner+0x10);
 const uint32_t now=sampleClock(sh),token=manualVersions.get(id);
 const int frame=get<int>(0xA8ED84),session=get<int>(0xA8B238);
 if(op==9){
  // This branch is only the displaced Owner predicate. The game's following
  // virtual +98 CanSell check remains in place and may still refuse the cursor.
  if(suppliedOwner!=owner)return 0;
  nativeSellCandidate={object,id,owner,ownerID,index,match,lastHouse,token,now,frame,session};
  ++sh->nativeSellCursorPasses;return 1;
 }
 const auto candidate=nativeSellCandidate;nativeSellCandidate={};
 if(candidate.object!=object||candidate.id!=id||candidate.owner!=owner||
    candidate.ownerID!=ownerID||candidate.index!=index||candidate.epoch!=match||
    candidate.local!=lastHouse||candidate.token!=token||candidate.session!=session||
    frame<candidate.frame||frame-candidate.frame>90||DWORD(now-candidate.tick)>1500)return 0;
 // Native OutList is the next consumer even in SP; do not check the toolbox's
 // SP DoList lane here or claim that the original append has already happened.
 const bool singleAI=eventdelivery::isSingleplayer(session)&&!sellHumanHouse(owner);
 if(!singleAI&&!queueAvailable(get<int>(0xA802C8),get<int>(0xA802D0)))return 0;
 actorFrameArbiter.begin(match,frame);
 if(!manualVersions.trackable(id)||!actorFrameArbiter.canIssue(id,token)||
    !actorLeaseArbiter.canIssue(match,frame,id,object,token,taskarbiter::Owner::Sell))return 0;
 const uint32_t table=get<uint32_t>(object),canSell=get<uint32_t>(table+0x98);
 static const uint8_t canSellHead[]={0x56,0x8B,0xF1,0xE8,0xF8,0x69,0x2C,0x00};
 if(canSell!=0x4494C0||!debugentries::pattern(debugentries::Kind::Call,0x4494C0,canSellHead,sizeof canSellHead,read)||
    !call<bool>(canSell,object))return 0;
 // The native legality query is a call boundary: recheck both identities and
 // permission immediately before substituting the constructor's House index.
 uint32_t checkedOwner=0,checkedIndex=0;
 if(!nativeSellCursorReady()||!nativeSellCursorTarget(object,checkedOwner,checkedIndex)||
    checkedOwner!=owner||checkedIndex!=index||get<uint32_t>(object+0x10)!=id||
    get<uint32_t>(owner+0x10)!=ownerID||manualVersions.get(id)!=token)return 0;
 if(singleAI){
  static constexpr uint8_t continuation[]={0x8D,0x4C,0x24,0x28,0x52,0xE8,0x54,0xA4,1,0};
  if(!debugentries::pattern(debugentries::Kind::Call,0x4AC182,continuation,sizeof continuation,read))return -1;
  Event packet{};if(!sellpacket::event(packet,int(index),id,uint32_t(frame))||!executeSinglePlayerAISell(packet,object)){sh->nativeSellCursorStatus=-5;return -1;}
  actorFrameArbiter.record(id,token,taskarbiter::Owner::Sell);++sh->nativeSellOwnerAdjusted;return -1;
 }
 ++sh->nativeSellOwnerAdjusted;
 return int(index)+1; // 0 means replay the original CurrentHouse load.
}
static __declspec(naked) void nativeSellCursorEntry(){__asm {
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
 push dword ptr [ebx+24]
 push dword ptr [ebx+4]
 push 9
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 mov dword ptr [ebx+28],1
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 ret
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 50B6F0h
 ret
}}
static __declspec(naked) void nativeSellOwnerEntry(){__asm {
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
 push dword ptr [ebx+4]
 push 10
 call GuardedRiskAction
 add esp,12
 cmp eax,-1
 je consumed
 test eax,eax
 jz original
 dec eax
 mov [ebx+20],eax
 mov eax,dword ptr ds:[0A83D4Ch]
 mov [ebx+24],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 4AC182h
 ret
consumed:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 add esp,12
 push 4AC20Ch
 ret
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov ecx,dword ptr ds:[0A83D4Ch]
 mov edx,[ecx+30h]
 push 4AC182h
 ret
}}
static RiskSite nativeSellSites[]={
 {experimentalpatch::NativeSellCursor,5,{0xE8,0x06,0x8D,0xE7,0xFF},{}},
 {experimentalpatch::NativeSellOwner,9,{0x8B,0x0D,0x4C,0x3D,0xA8,0x00,0x8B,0x51,0x30},{}}
};
static void prepareNativeSellCursor(){
 static bool prepared=false;if(prepared)return;prepared=true;
 const uint32_t targets[]={uint32_t(uintptr_t(&nativeSellCursorEntry)),uint32_t(uintptr_t(&nativeSellOwnerEntry))};
 for(unsigned i=0;i<2;++i){auto&s=nativeSellSites[i];memset(s.after,0x90,s.size);
  s.after[0]=i==0?0xE8:0xE9;const uint32_t offset=targets[i]-s.address-5;memcpy(s.after+1,&offset,4);}
}
static void restoreNativeSellCursor(){
 nativeSellCursorActive=false;nativeSellCandidate={};
 bool cursor=restoreRiskSite(nativeSellSites[0]),owner=restoreRiskSite(nativeSellSites[1]);
 if(sh)sh->nativeSellCursorStatus=cursor&&owner?0:-2;
}
static void nativeSellCursorTick(){
 if(!sh||!sh->nativeSellCursor){restoreNativeSellCursor();return;}
 if(!riskReady()||snap.chat||snap.status!=1){restoreNativeSellCursor();if(sh)sh->nativeSellCursorStatus=-3;return;}
 if(eventdelivery::isMultiplayer(get<int>(0xA8B238))&&!networkHousePreserveReady()){
  restoreNativeSellCursor();if(sh)sh->nativeSellCursorStatus=-4;return;
 }
 if(eventdelivery::isMultiplayer(get<int>(0xA8B238))&&!nativeSellQueueCompatible()){
  restoreNativeSellCursor();if(sh)sh->nativeSellCursorStatus=nativesell::QueueUnavailable;return;
 }
 prepareNativeSellCursor();nativeSellCursorActive=false;int failure=0;
 for(unsigned i=0;i<2;++i){auto&s=nativeSellSites[i];uint8_t bytes[16]{};const bool active=codeHookRegistry.active(s.address);
  const int base=i?nativesell::OwnerConflict:nativesell::CursorConflict;
  if(codeHookRegistry.conflict(s.address)){failure=base;break;}
  if(!patchcontext::compatible(s.address,[](uint32_t at,void*out,size_t n){return read(at,out,n);})){failure=base-1;break;}
  if(!read(s.address,bytes,s.size)){failure=base-2;break;}
  if(active?memcmp(bytes,s.after,s.size):!hookregistry::admissibleOriginal(s.address,bytes,s.size)){failure=base-3;break;}
 }
 // Install the House adapter first; publish active only after the complete pair.
 if(!failure&&!installRiskSite(nativeSellSites[1]))failure=nativesell::OwnerInstall;
 if(!failure&&!installRiskSite(nativeSellSites[0]))failure=nativesell::CursorInstall;
 if(failure){restoreNativeSellCursor();if(sh&&sh->nativeSellCursorStatus==0)sh->nativeSellCursorStatus=failure;return;}
 nativeSellCursorActive=true;sh->nativeSellCursorStatus=1;
}
