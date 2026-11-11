// Independent opt-in: preserve the source object's actual House in native
// outgoing commands. The original constructors and OutList writers still run.
// These three adapters run after construction but before any append; failure
// takes the original false-return path, never a local-House substitute.
static bool controlCommandOwnerNativeObjectReady();
static bool controlCommandOwnerActive;
static RiskSite controlCommandOwnerSites[]={
 {0x646EF5,10,{0x81,0x3D,0xC8,0x02,0xA8,0x00,0x80,0x00,0x00,0x00},{}},
 {0x646FD2,10,{0x81,0x3D,0xC8,0x02,0xA8,0x00,0x80,0x00,0x00,0x00},{}},
 {0x6FFC93,8,{0x83,0xEC,0x70,0xB9,0x1B,0x00,0x00,0x00},{}}
};
static bool controlCommandOwnerOwnSites(){
 if(!controlCommandOwnerActive)return false;
 for(const auto&s:controlCommandOwnerSites)
  if(!codeHookRegistry.active(s.address)||codeHookRegistry.conflict(s.address)||!riskSiteCurrent(s))return false;
 return true;
}
static bool controlCommandOwnerMegaCompatible(){
 const auto&s=controlCommandOwnerSites[0];uint8_t actual[10]{};
 if(!read(s.address,actual,sizeof actual))return false;
 if(!memcmp(actual,s.before,sizeof actual))return true;
 return codeHookRegistry.active(s.address)&&!codeHookRegistry.conflict(s.address)&&
  !memcmp(actual,s.after,sizeof actual);
}
static bool controlCommandOwnerReady(uint32_t owner){
 if(!sh||!sh->controlCommandOwner||!controlmode::native(sh->controlAll)||controlTransitionPending()||!sh->foreground||
    !riskReady(true)||snap.status!=1||snap.commandStatus!=1||get<bool>(0xA8BC60+25)||
    (!privateOutActive()&&(!controlCommandOwnerOwnSites()||!controlCommandOwnerNativeObjectReady()))||
    !playableHouse(owner))return false;
 const int index=get<int>(owner+0x30);const uint32_t identity=get<uint32_t>(owner+0x10);Vec houses=vec(0xA80228,32);
 if(index<0||index>=houses.count||item(houses,index)!=owner)return false;
 const bool transport=privateOutActive()||owner==lastHouse||!eventdelivery::isMultiplayer(get<int>(0xA8B238))||
  (networkHousePreserveReady()&&networkSenderHouse(index));
 return transport&&get<uint32_t>(owner+0x10)==identity&&get<int>(owner+0x30)==index&&item(houses,index)==owner;
}
// Shared read-only source resolution. Never infer allegiance from the target.
static bool controlCommandOwnerSource(uint32_t id,uint8_t tag,uint32_t&object,uint32_t&owner){
 if(!id||tag!=0x34)return false;
 indexWorld(get<int>(0xA8ED84));object=worldByID(id);
 if(!object||kind(object)<0||!read(object+0x21C,&owner,4)||owner<0x10000)return false;
 return true;
}
static int controlCommandOwnerAction(unsigned op,uint32_t packetAddress,uint32_t path){
 if(op!=13||path>2||!sh)return 0;
 auto reject=[](){++sh->controlCommandOwnerRejected;return 0;};
 uint8_t packet[30]{};
 if(!read(packetAddress,packet,sizeof packet)||packet[0]!=(path==1?5:4))return reject();
 uint32_t id=0,object=0,owner=0;memcpy(&id,packet+7,4);
 if(!controlCommandOwnerSource(id,packet[0xB],object,owner))return reject();
 if(owner==lastHouse&&owner==get<uint32_t>(0xA83D4C))return 1; // preserve own commands only in the same player context
 // Planning append and PlanCommit are a pair. The vanilla commit has no actor
 // and still names CurrentHouse, so this version rejects foreign planning at
 // its first append. Received planning and later local DoList are untouched.
 const int index=get<int>(owner+0x30);const uint32_t ownerID=get<uint32_t>(owner+0x10);
 if(path==2||packet[0xC]>=32||!live(object)||!controlCommandOwnerReady(owner))return reject();
 networksender::Ring queue{};if(!networkSenderQueue(queue))return reject();
 // Revalidate source identity immediately before writing the local Event.
 if(worldByID(id)!=object||get<uint32_t>(object+0x21C)!=owner||
    get<uint32_t>(object+0x10)!=id||get<uint32_t>(owner+0x10)!=ownerID||get<int>(owner+0x30)!=index)return reject();
 *(uint8_t*)(uintptr_t(packetAddress)+2)=uint8_t(index);
 // Counts successful identity preparation, not receiver execution. The
 // original capacity branch remains authoritative and returns false at 128.
 if(queue.count<128)++sh->controlCommandOwnerAccepted;
 return 1;
}

static __declspec(naked) void controlCommandOwnerMegaEntry(){__asm {
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
 push dword ptr [ebx+28]
 push 13
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz denied
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 cmp dword ptr ds:[0A802C8h],80h
 push 646EFFh
 ret
denied:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 646F58h
 ret
}}
static __declspec(naked) void controlCommandOwnerMegaFEntry(){__asm {
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
 push dword ptr [ebx+28]
 push 13
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz denied
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 cmp dword ptr ds:[0A802C8h],80h
 push 646FDCh
 ret
denied:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 647035h
 ret
}}
static __declspec(naked) void controlCommandOwnerPlanningEntry(){__asm {
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
 push 2
 push dword ptr [ebx+28]
 push 13
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz denied
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 sub esp,70h
 mov ecx,1Bh
 push 6FFC9Bh
 ret
denied:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 6FFCAEh
 ret
}}
static void prepareControlCommandOwner(){
 static bool prepared=false;if(prepared)return;prepared=true;
 const uint32_t targets[]={uint32_t(uintptr_t(&controlCommandOwnerMegaEntry)),
  uint32_t(uintptr_t(&controlCommandOwnerMegaFEntry)),uint32_t(uintptr_t(&controlCommandOwnerPlanningEntry))};
 for(unsigned n=0;n<3;++n){auto&s=controlCommandOwnerSites[n];
  memset(s.after,0x90,s.size);s.after[0]=0xE9;uint32_t offset=targets[n]-s.address-5;memcpy(s.after+1,&offset,4);
 }
}
static void restoreControlCommandOwner(){
 controlCommandOwnerActive=false;bool ok=true;
 for(auto&s:controlCommandOwnerSites)if(!restoreRiskSite(s))ok=false;
 if(sh)sh->controlCommandOwnerStatus=ok?0:-2;
}
static void controlCommandOwnerTick(){
 if(!sh||!sh->controlCommandOwner||!controlmode::native(sh->controlAll)||controlTransitionPending()){restoreControlCommandOwner();return;}
 if(!riskReady()){restoreControlCommandOwner();sh->controlCommandOwnerStatus=-2;return;}
 prepareControlCommandOwner();controlCommandOwnerActive=false;bool ok=true;
 for(auto&s:controlCommandOwnerSites)if(!installRiskSite(s)){ok=false;break;}
 if(!ok){restoreControlCommandOwner();sh->controlCommandOwnerStatus=-2;return;}
 controlCommandOwnerActive=true;
 const bool transport=!eventdelivery::isMultiplayer(get<int>(0xA8B238))||networkHousePreserveReady();
 sh->controlCommandOwnerStatus=controlmode::native(sh->controlAll)&&transport&&controlCommandOwnerNativeObjectReady()?1:-2;
}
