// Original Techno::ClickedEvent keeps its constructor, planning branch and
// OutList writer. Only its actor-derived House argument is adapted here.
static bool controlCommandOwnerReady(uint32_t owner);
static bool controlObjectOwnerActive;
static RiskSite controlObjectOwnerSite{0x6FFE2B,5,{0x8B,0x42,0x30,0x51,0x50},{}};

static bool controlCommandOwnerNativeObjectReady(){
 const auto&s=controlObjectOwnerSite;
 return controlObjectOwnerActive&&codeHookRegistry.active(s.address)&&
  !codeHookRegistry.conflict(s.address)&&!memcmp((const void*)s.address,s.after,s.size);
}
static bool controlObjectEventKind(unsigned kind){
 // These receiver branches resolve event+7 as the subject object. Global,
 // production, House-only and planning events must not be treated as units.
 switch(kind){
 case 0x01:case 0x02: // building power
 case 0x06:case 0x07: // Idle / Scatter
 case 0x09:case 0x0A: // Deploy / Detonate
 case 0x11:case 0x15:case 0x16: // Primary / Repair / Sell
  return true;
 default:return false;
 }
}
static int controlObjectOwnerAction(unsigned op,uint32_t sourceAddress,uint32_t eventKind){
 auto reject=[](){if(sh)++sh->controlCommandOwnerRejected;return 0;};
 if(op!=14||!sh||!lastHouse)return reject();
 uint8_t source[5]{};uint32_t id=0;
 if(!read(sourceAddress,source,sizeof source)||source[4]!=0x34)return reject();
 memcpy(&id,source,4);if(!id)return reject();
 const int frame=get<int>(0xA8ED84);if(frame<0||frame>=INT32_MAX)return reject();
 indexWorld(frame);
 const uint32_t object=worldByID(id);
 if(!object||!worldObjects.contains(object)||!live(object)||kind(object)<0||
    get<uint32_t>(object+0x10)!=id)return reject();
 const uint32_t owner=get<uint32_t>(object+0x21C);
 if(!playableHouse(owner))return reject();
 const int index=get<int>(owner+0x30);const Vec houses=vec(0xA80228,32);
 if(index<0||index>=houses.count||item(houses,index)!=owner)return reject();
 // An ordinary own-object call keeps every native event kind and the native
 // planning behavior. The foreign opt-in must not narrow that existing path.
 if(owner==lastHouse&&owner==get<uint32_t>(0xA83D4C))return index+1;
 if(!controlObjectEventKind(eventKind)||!controlCommandOwnerReady(owner)||
    get<bool>(0xAC4CF4))return reject();
 // Original ClickedEvent returns true even at a full queue. Report preparation
 // only when the native append can occur, never execution or remote completion.
 networksender::Ring queue{};
 if(!networkSenderQueue(queue)||queue.count>=128)return reject();
 ++sh->controlCommandOwnerAccepted;return index+1;
}

// 6FFE2B: EDX=CurrentHouse, ECX=event kind, [ESP]=source NetID (5 bytes,
// two stack slots). The displaced instructions load/push House and kind.
// Reject before construction and unwind the temporary NetID into the native
// false epilogue; never leave a foreign command addressed to CurrentHouse.
static __declspec(naked) void controlObjectOwnerEntry(){__asm {
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
 lea eax,[ebx+36]
 push eax
 push 14
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz rejected
 dec eax
 mov [ebx+28],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push ecx
 push eax
 push 6FFE30h
 ret
rejected:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 add esp,8
 push 6FFE4Bh
 ret
}}
static void restoreControlObjectOwner(){
 controlObjectOwnerActive=false;restoreRiskSite(controlObjectOwnerSite);
}
static void controlObjectOwnerTick(){
 if(!sh||!sh->controlCommandOwner||!controlmode::native(sh->controlAll)||controlHouseFlagsPending()||!riskReady()){restoreControlObjectOwner();return;}
 auto&s=controlObjectOwnerSite;
 if(!s.after[0]){s.after[0]=0xE9;const uint32_t offset=uint32_t(uintptr_t(&controlObjectOwnerEntry))-s.address-5;memcpy(s.after+1,&offset,4);}
 controlObjectOwnerActive=installRiskSite(s);
 if(!controlObjectOwnerActive)restoreControlObjectOwner();
}
