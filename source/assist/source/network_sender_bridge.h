// Native sender adaptation. All executable writes go through GameEntryManager.
// Include after player_test_bridge.h; the outer guard owns one Capture lease.
#include "network_sender_policy.h"
#include "../../shared/source/runtime_fp_scope.h"
extern "C" int __cdecl GuardedNetworkSend(unsigned,unsigned,int,int,int);
static bool networkSenderInstalled,networkSenderEnabled,networkSenderPacking,networkSenderFaulted;
static bool networkSenderCacheDirty;
static int networkSenderPacketLimit,networkSenderPacketHouse=-1;
static uint32_t networkSenderScenario,networkSenderItems,networkSenderLocal,networkSenderLocalID,networkSenderMatch;
static int networkSenderState=networksender::Off;
struct NetworkSenderSite {networksender::Site spec;uint8_t patch[10];};
static NetworkSenderSite networkSenderSites[networksender::SiteCount];
static bool networkSenderQueue(networksender::Ring&r){return read(0xA802C8,&r,sizeof r)&&networksender::valid(r);}
static uint32_t networkSenderEvent(unsigned slot){return 0xA802D4+slot*networksender::EventBytes;}
static bool networkSenderWorld(){
 uint32_t scenario=0,items=0,local=0,id=0;
 return networkSenderScenario&&networkSenderMatch==match&&
  read(0xA8B230,&scenario,4)&&scenario==networkSenderScenario&&
  read(0xA8022C,&items,4)&&items==networkSenderItems&&
  read(0xA83D4C,&local,4)&&local==networkSenderLocal&&
  local>=0x10000&&read(local+0x10,&id,4)&&id==networkSenderLocalID;
}
static void networkSenderBindWorld(){
 networkSenderScenario=get<uint32_t>(0xA8B230);networkSenderItems=get<uint32_t>(0xA8022C);
 networkSenderLocal=get<uint32_t>(0xA83D4C);networkSenderLocalID=get<uint32_t>(networkSenderLocal+0x10);
 networkSenderMatch=match;
}
static bool networkSenderHouse(int index){
 if(!networksender::validHouse(index))return false;
 Vec houses=vec(0xA80228,32);if(index>=houses.count)return false;
 uint32_t house=item(houses,index);
 return house>=0x10000&&get<int>(house+0x30)==index&&playableHouse(house)&&
  (get<uint8_t>(house+0x1EC)||get<uint8_t>(house+0x1ED));
}
static bool networkSenderOwnSites(){
 for(const auto&s:networkSenderSites){uint8_t actual[10]{};
  if(!codeHookRegistry.active(s.spec.address)||codeHookRegistry.conflict(s.spec.address)||
     !read(s.spec.address,actual,s.spec.size)||memcmp(actual,s.patch,s.spec.size))return false;
 }return true;
}
static bool networkSenderContext();
static bool networkSenderDeliveryReady(){
 return networkSenderEnabled&&networkSenderInstalled&&!networkSenderFaulted&&sh&&!sh->stop&&!fault&&
  sh->networkHousePreserve&&controlmode::sender(sh->controlAll)&&!controlTransitionPending()&&match&&snap.match==match&&snap.status==1&&
  (snap.commandStatus==activation::Ready||snap.commandStatus==activation::DisplayHidden)&&
  readonly::commands(sh)&&!readOnlyState.active&&
  DWORD(sampleClock(sh)-sh->heartbeat)<3000&&get<bool>(0xA8E9A0)&&
  snap.session==get<int>(0xA8B238)&&eventdelivery::isMultiplayer(snap.session)&&
  networksender::protocol(get<int>(0xA8B24C))&&networkSenderWorld()&&networkSenderOwnSites();
}
static bool networkHousePreserveReady(){return snap.commandStatus==activation::Ready&&networkSenderDeliveryReady();}
static void networkSenderPublish(){if(sh)sh->networkHouseStatus=networkSenderState;}
static bool networkSenderClearCache(){
 if(!networkSenderCacheDirty)return true;
 // Local encoder metadata only. Never clear the receiver's House-indexed cache.
 if(!gameentry::active(gameentry::Client::Assist))return false;
 memset((void*)0xAFA338,0,8*sizeof(uint32_t));networkSenderCacheDirty=false;return true;
}
// Only unsent OutList entries are cancelled; DoList and already sent packets
// are untouched. Queue order and matching timestamp slots are preserved.
static bool networkSenderCancelForeign(bool invalidOnly=false){
 if(!gameentry::active(gameentry::Client::Assist)||!networkSenderWorld())return false;
 networksender::Ring ring{},again{};if(!networkSenderQueue(ring))return false;
 int local=-1;
 if(!read(networkSenderLocal+0x30,&local,4)||!networksender::validHouse(local))return false;
 uint8_t events[128][111]{};uint32_t times[128]{};unsigned kept=0,dropped=0;
 for(int i=0;i<ring.count;++i){const unsigned slot=networksender::slot(ring,unsigned(i));
  uint8_t event[111]{};uint32_t time=0;
  if(!read(networkSenderEvent(slot),event,sizeof event)||!read(0xA83A54+slot*4,&time,4))return false;
  const int house=int(int8_t(event[2]));const bool foreign=networksender::foreign(house,local);
  if(foreign&&(!invalidOnly||!networksender::gameplay(event[0])||!networkSenderHouse(house))){++dropped;continue;}
  memcpy(events[kept],event,sizeof event);times[kept++]=time;
 }
 if(!networkSenderQueue(again)||!networksender::same(ring,again))return false;
 if(!dropped)return true;
 for(unsigned i=0;i<kept;++i){const unsigned slot=networksender::slot(ring,i);
  memcpy((void*)(uintptr_t)networkSenderEvent(slot),events[i],111);
  *(uint32_t*)(0xA83A54+slot*4)=times[i];
 }
 // Head remains fixed, including at ring wrap. No pointer from an old match is used.
 *(int*)0xA802D0=(ring.head+int(kept))&127;*(int*)0xA802C8=int(kept);
 if(sh)sh->networkHouseDropped+=dropped;return true;
}
static bool networkSenderNoForeign(){
 networksender::Ring ring{};if(!networkSenderQueue(ring))return false;
 if(!ring.count)return true;
 uint32_t house=0;int local=-1;
 if(!read(0xA83D4C,&house,4)||house<0x10000||!read(house+0x30,&local,4)||
    !networksender::validHouse(local))return false;
 for(int i=0;i<ring.count;++i){int8_t owner=-1;
  if(!read(networkSenderEvent(networksender::slot(ring,unsigned(i)))+2,&owner,1)||int(owner)!=local)return false;
 }return true;
}
// The native entry and ret12 ABI remain intact; only its initial five bytes
// are replayed here. Original code owns framing, timestamps, counts and sends.
static __declspec(naked) int __fastcall networkSenderOriginal(unsigned,unsigned,int,int,int){__asm {
 sub esp,1Ch
 push ebx
 push ebp
 push 649CA5h
 ret
}}
extern "C" int __cdecl AssistNetworkSend(unsigned transport,unsigned buffer,int capacity,int delay,int serial){
 const bool requested=networkSenderDeliveryReady()&&networkSenderContext();
 if(!requested){
  networkSenderEnabled=false;
  // A changed world is never edited using the preceding world's identity.
  if((networkSenderWorld()&&!networkSenderCancelForeign())||
     (!networkSenderWorld()&&!networkSenderNoForeign())||!networkSenderClearCache()){
   networkSenderState=networksender::Blocked;networkSenderPublish();return 0;
  }
 }else if(!networkSenderCancelForeign(true)){
  networkSenderFaulted=true;networkSenderState=networksender::Blocked;networkSenderPublish();return 0;
 }
 const bool previousPacking=networkSenderPacking;
 networkSenderCacheDirty=networkSenderCacheDirty||requested;
 const uint32_t before=get<uint32_t>(0xB0487C);int sent=0;
 runtimeunwind::run([&]{networkSenderPacking=requested;sent=networkSenderOriginal(transport,buffer,capacity,delay,serial);},[&]{networkSenderPacking=previousPacking;});
 if(networkSenderFaulted){networkSenderState=networksender::Blocked;networkSenderPublish();}
 if(sh)sh->networkHousePackets+=get<uint32_t>(0xB0487C)-before;
 return sent;
}
extern "C" void __cdecl AssistNetworkSenderAbort(){
 networkSenderPacking=false;networkSenderEnabled=false;networkSenderFaulted=true;
 networkSenderState=networksender::Blocked;
 // This is an SEH handler: never touch the world, queue or Shared pointer that
 // may have caused the first fault. Keep the gate installed. A later ordinary
 // guarded entry/maintenance may publish status and cancel unsent work safely.
}
static int __fastcall networkSenderEntry(unsigned transport,unsigned buffer,int capacity,int delay,int serial){
 RuntimeFPScope fp;int result=0;runtimeunwind::run([&]{result=GuardedNetworkSend(transport,buffer,capacity,delay,serial);},[&]{fp.restore();});return result;
}
// Called after the original FrameInfo has filled Frame/CRC/count/Delay, before
// either encoder. Only House changes. CRC is the original global game-state
// digest, not a per-House checksum or a checksum of these packet bytes.
static bool __cdecl networkSenderFrameGroup(uint32_t stack,uint32_t buffer){
 if(!networkSenderPacking)return true;
 networkSenderPacketLimit=0;networkSenderPacketHouse=-1;
 networksender::Ring ring{};if(!networkSenderQueue(ring)||!networkSenderWorld())return false;
 const int limit=*(int*)(uintptr_t)(stack+0x10);if(limit<0)return false;
 if(!ring.count||!limit)return true;
 const int local=get<int>(networkSenderLocal+0x30);int first=-1;unsigned count=0;
 for(int i=0;i<ring.count&&i<limit;++i){uint8_t head[3]{};
  if(!read(networkSenderEvent(networksender::slot(ring,unsigned(i))),head,sizeof head))return false;
  int house=int(int8_t(head[2]));
  if(house!=local&&(!networksender::gameplay(head[0])||!networkSenderHouse(house)))return false;
  if(!i)first=house;else if(house!=first)break;++count;
 }
 if(!count||!networksender::validHouse(first)||buffer<0x10000)return false;
 networksender::Ring again{};if(!networkSenderQueue(again)||!networksender::same(ring,again))return false;
 networkSenderPacketLimit=int(count);networkSenderPacketHouse=first;
 *(uint8_t*)(uintptr_t)(buffer+2)=uint8_t(first);
 return true;
}
static bool __cdecl networkSenderCompressedGroup(uint32_t stack,uint32_t buffer){
 if(!networkSenderPacking)return true;
 const int limit=*(int*)(uintptr_t)(stack+12);if(limit<0)return false;
 if(!limit)return true;
 if(networkSenderPacketLimit<=0||!networksender::validHouse(networkSenderPacketHouse)||
    buffer<0x10000||get<uint8_t>(buffer+2)!=networkSenderPacketHouse)return false;
 *(int*)(uintptr_t)(stack+12)=std::min(limit,networkSenderPacketLimit);return true;
}
static __declspec(naked) void networkSenderFrameEntry(){__asm {
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
 push dword ptr [ebx+16]
 mov eax,[ebx+12]
 add eax,4
 push eax
 call networkSenderFrameGroup
 add esp,8
 test al,al
 jz blocked
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov eax,dword ptr ds:[0A8B24Ch]
 push 649D66h
 ret
blocked:
 mov byte ptr [networkSenderFaulted],1
 mov byte ptr [networkSenderEnabled],0
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 649FD4h
 ret
}}
// Bound the original protocol0 inner loop without changing the real queue,
// the outer remaining count, native progress accounting or return convention.
static __declspec(naked) void networkSenderPlainLimitEntry(){__asm {
 mov edx,[esp+14h]
 mov esi,[esp+10h]
 cmp byte ptr [networkSenderPacking],0
 je original
 cmp esi,dword ptr [networkSenderPacketLimit]
 jle original
 mov esi,dword ptr [networkSenderPacketLimit]
original:
 // The original CMP immediately after this continuation defines live flags.
 push 649DC4h
 ret
}}
static __declspec(naked) void networkSenderCompressedEntry(){__asm {
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
 mov eax,[ebx+12]
 add eax,4
 push eax
 call networkSenderCompressedGroup
 add esp,8
 test al,al
 jz blocked
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 sub esp,94h
 push 64B666h
 ret
blocked:
 mov byte ptr [networkSenderFaulted],1
 mov byte ptr [networkSenderEnabled],0
 // Return to the native outer epilogue with its previous packets' consumed
 // count. The current group has not consumed any event and is not sent.
 mov eax,[ebx+12]
 mov dword ptr [eax+4],649FD4h
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 ret 16
}}
static __declspec(naked) void networkSenderUncompressedHouse(){__asm {
 pushfd
 cmp byte ptr [networkSenderPacking],0
 jne preserved
 popfd
 mov byte ptr [eax+eax*2+0A802D6h],cl
 push 649E25h
 ret
preserved:
 popfd
 push 649E25h
 ret
}}
static __declspec(naked) void networkSenderCompressedHouse(){__asm {
 pushfd
 cmp byte ptr [networkSenderPacking],0
 jne preserved
 popfd
 mov byte ptr [eax+eax*2+0A802D6h],cl
 push 64B9DBh
 ret
preserved:
 popfd
 push 64B9DBh
 ret
}}
static __declspec(naked) void networkSenderCacheEntry(){__asm {
 pushfd
 cmp byte ptr [networkSenderPacking],0
 jne fullGroup
 popfd
 mov ecx,edx
 mov dword ptr [esp+10h],0
 push 64B4F4h
 ret
fullGroup:
 popfd
 push 64B551h
 ret
}}
static void networkSenderPrepare(){static bool prepared=false;if(prepared)return;prepared=true;
 const uint32_t targets[]={uint32_t(uintptr_t(&networkSenderEntry)),uint32_t(uintptr_t(&networkSenderUncompressedHouse)),
  uint32_t(uintptr_t(&networkSenderCompressedEntry)),uint32_t(uintptr_t(&networkSenderCompressedHouse)),uint32_t(uintptr_t(&networkSenderCacheEntry)),
  uint32_t(uintptr_t(&networkSenderFrameEntry)),uint32_t(uintptr_t(&networkSenderPlainLimitEntry))};
 for(unsigned i=0;i<networksender::SiteCount;++i){auto&s=networkSenderSites[i];s.spec=networksender::Sites[i];memset(s.patch,0x90,s.spec.size);s.patch[0]=0xE9;
  const uint32_t rel=targets[i]-s.spec.address-5;memcpy(s.patch+1,&rel,4);
 }
}
static bool networkSenderContext(){
 // Fixed cross-site ABI dependencies; the manager also checks each exact
 // displaced instruction and refuses any foreign detour at our seven sites.
 struct Probe {uint32_t site,address;uint8_t size,bytes[16];};
 static const Probe probes[]={
  {0x649CA0,0x649CA5,6,{0x8B,0x2D,0xF8,0x41,0x8B,0x00}},
  {0x649CA0,0x649D40,12,{0x8B,0x15,0x4C,0x3D,0xA8,0x00,0x8A,0x42,0x30,0x88,0x43,0x02}},
  {0x649CA0,0x649D4C,11,{0x8B,0x15,0xFC,0x51,0xAC,0x00,0x66,0x8B,0x44,0x24,0x38}},
  {0x649CA0,0x649D57,10,{0x89,0x53,0x07,0x66,0x89,0x43,0x0B,0x88,0x4B,0x0D}},
  {0x649D61,0x649D8D,7,{0x8B,0xCB,0xE8,0xCC,0x18,0x00,0x00}},
  {0x649DBC,0x649DC4,8,{0x3B,0xD6,0x0F,0x8D,0x8F,0x01,0x00,0x00}},
  {0x649CA0,0x649FF0,12,{0x8B,0xC6,0x5F,0x5E,0x5D,0x5B,0x83,0xC4,0x1C,0xC2,0x0C,0x00}},
  {0x64B660,0x64B666,7,{0x53,0x55,0x56,0x8B,0xD9,0x57,0xB9}},
  {0x64B4EA,0x64B551,10,{0x8A,0x44,0x24,0x30,0x84,0xC0,0x0F,0x84,0xF4,0x00}},
  {0x64B9D4,0x64BDD0,9,{0x81,0xEC,0x9C,0x00,0x00,0x00,0x53,0x55,0x56}},
  {0x64B9D4,0x64BE32,10,{0x8A,0x47,0x02,0x8B,0x4F,0x03,0x88,0x44,0x24,0x3E}}
 };
 for(const auto&p:probes){uint8_t bytes[16]{};const bool readable=read(p.address,bytes,p.size);if(!debugentries::allow(debugentries::Kind::Patch,p.site,readable,!memcmp(bytes,p.bytes,p.size)))return false;}
 return true;
}
static bool restoreNetworkSender(){
 networkSenderEnabled=false;networkSenderPrepare();
 if(networkSenderPacking){networkSenderState=networksender::Draining;networkSenderPublish();return false;}
 const bool any=networkSenderInstalled||codeHookRegistry.active(networksender::Sender);
 if(any&&((networkSenderWorld()&&!networkSenderCancelForeign())||
          (!networkSenderWorld()&&!networkSenderNoForeign())||!networkSenderClearCache())){
  networkSenderState=networksender::Blocked;networkSenderPublish();return false;
 }
 bool ok=true;for(int i=int(networksender::SiteCount)-1;i>=0;--i){auto&s=networkSenderSites[i];if(codeHookRegistry.active(s.spec.address)&&
  !gameentry::patch(gameentry::Client::Assist,s.spec.address,s.patch,s.spec.original,s.spec.size))ok=false;}
 networkSenderInstalled=!ok;networkSenderState=ok?networksender::Off:networksender::Unsupported;networkSenderPublish();return ok;
}
static void networkSenderTick(bool requested){
 networkSenderPrepare();
 requested=requested&&sh&&controlmode::sender(sh->controlAll)&&!controlTransitionPending()&&!sh->stop&&!fault&&eventdelivery::isMultiplayer(get<int>(0xA8B238))&&
  lastHouse&&lastHouse==get<uint32_t>(0xA83D4C)&&get<bool>(0xA8E9A0)&&networksender::protocol(get<int>(0xA8B24C));
 if(!requested){restoreNetworkSender();return;}
 if(networkSenderPacking)return;
 if(networkSenderInstalled&&!networkSenderWorld()){
  // Do not adopt a new match and then treat its queue as the previous match's
  // pending work. Only an independently verified queue with no foreign owner
  // permits the old group/cache to retire before a fresh activation.
  networkSenderEnabled=false;
  if(!restoreNetworkSender())return;
 }
 // A sender fault remains latched in its own epoch. It must not disable
 // every later match in the pinned DLL after the old group retired safely.
 if(networkSenderFaulted&&networkSenderMatch!=match&&!networkSenderInstalled&&
    !codeHookRegistry.active(networksender::Sender)&&!networkSenderCacheDirty&&networkSenderNoForeign())networkSenderFaulted=false;
 bool ok=!networkSenderFaulted&&networkSenderContext();
 for(const auto&s:networkSenderSites){uint8_t bytes[10]{};const bool active=codeHookRegistry.active(s.spec.address);
  if(codeHookRegistry.conflict(s.spec.address)||!read(s.spec.address,bytes,s.spec.size)||(active?memcmp(bytes,s.patch,s.spec.size):!hookregistry::admissibleOriginal(s.spec.address,bytes,s.spec.size)))ok=false;
 }
 if(!ok){restoreNetworkSender();networkSenderState=networksender::Unsupported;networkSenderPublish();return;}
 if(!networkSenderWorld())networkSenderBindWorld();
 // Install the outer safety gate first. No new foreign admission is published
 // until every leaf has installed and the complete group is rechecked.
 for(auto&s:networkSenderSites)if(!codeHookRegistry.active(s.spec.address)&&
  !gameentry::patch(gameentry::Client::Assist,s.spec.address,s.spec.original,s.patch,s.spec.size)){ok=false;break;}
 networkSenderInstalled=ok&&networkSenderOwnSites();networkSenderEnabled=networkSenderInstalled;
 if(!networkSenderInstalled){restoreNetworkSender();networkSenderState=networksender::Unsupported;}
 else networkSenderState=networksender::Active;
 networkSenderPublish();
}
