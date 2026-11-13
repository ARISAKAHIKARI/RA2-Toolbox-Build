#pragma once
#include "private_outlist_sites.h"
// The original path is untouched in mode 0. Three cold-installed interception
// sites retain native constructors and UI/planning cleanup, but bypass their
// inline OutList writers. The exception handler only redirects to these stubs.
static bool privateBpRequest(bool);static bool privateBpRetired();static int privateBpStatus();
static privateout::Queue privateQueue;
static uint32_t privateMode=0,privateEpoch=0,privateRetry=0;
static bool privateEnabled=false,privateCompatible=false;
static DWORD privateOwner=0,privateChecked=0;
static int privateDispatchFrame=-1,privateDispatchBudget=0;
static unsigned privatePruneBudget=0;
static bool privateOutActive(){return privateEnabled&&sh&&sh->privateOutMode==privateMode&&!sh->stop&&!fault;}
static bool privateOutPending(){return privateQueue.count!=0;}
static int privateOutRoom(){return privateOutActive()?int(privateout::Capacity-privateQueue.count):0;}
static void privateStatus(int value){if(sh){sh->privateOutPending=privateQueue.count;sh->privateOutStatus=value;}}
static bool privateDestructHouse(int index,uint32_t house,uint32_t id){
 Vec houses=vec(0xA80228,32);
 return index>=0&&index<houses.count&&house>=0x10000&&id&&item(houses,index)==house&&
  get<uint32_t>(house+0x10)==id&&get<int>(house+0x30)==index&&playableHouse(house)&&!get<bool>(house+0x1F7)&&
  (!eventdelivery::isMultiplayer(get<int>(0xA8B238))||networkSenderHouse(index));
}
static bool privateOutQueue(const Event&source,bool manual){
 if(!privateOutActive()||GetCurrentThreadId()!=privateOwner||!riskReady(true)||!readonly::commands(sh)||readOnlyState.active)return false;
 privateout::Pending p{};if(!privateout::canonical(p.event,source))return false;
 const int frame=get<int>(0xA8ED84);const uint32_t id=privateout::actor(p.event);
 if(p.event.kind==vanilla::EventType::Destruct){
  Vec houses=vec(0xA80228,32);p.stamp.targetHouse=item(houses,int(p.event.house));
  p.stamp.targetID=p.stamp.targetHouse>=0x10000?get<uint32_t>(p.stamp.targetHouse+0x10):0;
  if(!privateDestructHouse(int(p.event.house),p.stamp.targetHouse,p.stamp.targetID)){++sh->privateOutRejected;privateStatus(-7);return false;}
 }
 if(id&&p.event.payload[4]==0x34){
  indexWorld(frame);const uint32_t actor=worldByID(id);
  if(!actor||!live(actor)){++sh->privateOutRejected;privateStatus(-7);return false;}
  p.stamp.actorPointer=actor;p.stamp.actorID=id;p.stamp.owner=get<uint32_t>(actor+0x21C);
  if(p.stamp.owner<0x10000){++sh->privateOutRejected;return false;}
  p.stamp.ownerID=get<uint32_t>(p.stamp.owner+0x10);
  if(privateout::objectKind(p.event.kind)&&sh->controlCommandOwner&&p.stamp.owner!=lastHouse&&controlCommandOwnerReady(p.stamp.owner)&&
   (!eventdelivery::isMultiplayer(snap.session)||sellHumanHouse(p.stamp.owner)))p.event.house=int8_t(get<int>(p.stamp.owner+0x30));
  // Full Mega groups use the actual sender's dictionary, including when its
  // actor belongs to another House. Never collide with that House's sender.
  if(p.event.kind==4||p.event.kind==5){
   if(eventdelivery::isMultiplayer(snap.session))p.event.house=int8_t(get<int>(lastHouse+0x30));
   else if(sh->controlCommandOwner)p.event.house=int8_t(get<int>(p.stamp.owner+0x30));
  }
  if(p.event.kind==0x16&&eventdelivery::isMultiplayer(snap.session)&&!sellHumanHouse(p.stamp.owner)){++sh->privateOutRejected;privateStatus(-7);return false;}
 }
 p.event.frame=uint32_t(frame);p.stamp.epoch=match;p.stamp.house=lastHouse;p.stamp.scenario=get<uint32_t>(0xA8B230);p.stamp.at=sampleClock(sh);p.manual=manual||p.event.kind==vanilla::EventType::Destruct;
 if(privateQueue.count==privateout::Capacity){++sh->privateOutRejected;privateStatus(-6);return false;}
 if(manual&&p.stamp.actorPointer&&!nativeMegaQueueCallDepth&&!nativeObjectQueueCallDepth)cancelNative(p.stamp.actorPointer,false);
 p.stamp.revision=manualVersions.get(p.stamp.actorID);
 if(!privateQueue.push(p)){++sh->privateOutRejected;privateStatus(-6);return false;}
 ++sh->privateOutAccepted;privateStatus(1);
 // Explicit manual input supersedes tool automation without calling an
 // immediate simulation setter. Own constructed requests have done this.
 return true;
}
static bool privateValid(const privateout::Pending&p){
 if(p.stamp.epoch!=match||p.stamp.house!=lastHouse||p.stamp.scenario!=get<uint32_t>(0xA8B230)||(!p.manual&&DWORD(sampleClock(sh)-p.stamp.at)>3000))return false;
 if(p.event.kind==vanilla::EventType::Destruct&&!privateDestructHouse(int(p.event.house),p.stamp.targetHouse,p.stamp.targetID))return false;
 const uint32_t a=p.stamp.actorPointer;if(!a)return true;
 return (p.manual||manualVersions.get(p.stamp.actorID)==p.stamp.revision)&&worldByID(p.stamp.actorID)==a&&live(a)&&get<uint32_t>(a+0x10)==p.stamp.actorID&&get<uint32_t>(a+0x21C)==p.stamp.owner&&get<uint32_t>(p.stamp.owner+0x10)==p.stamp.ownerID;
}
static void servicePrivateOutBatch(){
 if(!privateOutActive()||!privateQueue.count||!simulationReady(false,true)||snap.chat)return;
 if(privateRetry&&int32_t(sampleClock(sh)-privateRetry)<0)return;
 // Old native commands must leave first. We never move or edit that list.
 if(get<int>(0xA802C8)!=0){privateStatus(2);return;}
 const unsigned cancelled=privateout::prune(privateQueue,privatePruneBudget,privateValid);
 if(cancelled){sh->privateOutCancelled+=cancelled;privateStatus(-7);}
 if(!privatePruneBudget)return;
 if(!privateQueue.count)return;
 const int count=get<int>(0x8B41F8),tail=get<int>(0x8B4200),frame=get<int>(0xA8ED84);
 if(!eventdelivery::singleRoom(count,tail)||privateDispatchBudget<=0)return;
 Event events[privateout::BatchCapacity]{};unsigned n=0;
 while(n<privateQueue.count&&n<privateout::BatchCapacity&&n<unsigned(privateDispatchBudget)&&n<unsigned(16384-count)){
  const auto&p=privateQueue.at(n);if(!privateValid(p)||p.event.house!=privateQueue.at(0).event.house)break;events[n++]=p.event;
 }
 if(!n)return;
 uint32_t timing=0;if(!gameSendTiming(timing)||!simulationReady(false,true))return;
 const bool network=eventdelivery::isMultiplayer(snap.session);privateout::Wire wire{};
 const int protocol=get<int>(0xA8B24C),delay=get<int>(0xA8B550),interval=get<int>(0xA8B554);
 const uint16_t serial=get<uint16_t>(0xAFA400);const uint32_t crc=get<uint32_t>(0xAC51FC);
 constexpr uint32_t transport=0xA8E9C0;
 if(network){
  if(get<uint32_t>(0xB04474+(uint32_t(frame)&255)*4)!=crc||(protocol==2&&interval>0&&frame%interval))return;
  if(!privateout::encode(wire,events,n,protocol,uint32_t(frame),unsigned(delay),unsigned(interval),crc,serial)){privateStatus(-5);return;}
  if(get<uint32_t>(transport)!=0x7EC16C||get<uint32_t>(0x7EC174)!=0x5414C0||get<uint32_t>(0x7EC170)!=0x541820||!nativeExecutableRange(0x5414C0,1)||!nativeExecutableRange(0x541820,1)||
   !get<uint8_t>(transport+4)||!get<uint8_t>(transport+5)||get<int>(transport+0x44)<1||get<int>(transport+0x44)>8){privateStatus(-5);return;}
 }else{
  wire.count=n;for(unsigned i=0;i<n;++i){wire.local[i]=events[i];wire.local[i].frame=uint32_t(frame)+1;}
 }
 // All external callbacks precede this commit boundary. No allocation or
 // callback occurs between transport acceptance and queue retirement.
 if(!simulationReady(false,true)||get<int>(0x8B41F8)!=count||get<int>(0x8B4200)!=tail||get<int>(0xA8ED84)!=frame||
   (network&&(get<uint16_t>(0xAFA400)!=serial||get<int>(0xA8B24C)!=protocol||get<int>(0xA8B550)!=delay||get<int>(0xA8B554)!=interval||get<uint32_t>(0xAC51FC)!=crc)))return;
 for(unsigned i=0;i<n;++i)if(!privateValid(privateQueue.at(i)))return;
 if(network&&!call<int>(0x5414C0,transport,wire.bytes,int(wire.size),1,-1)){privateRetry=sampleClock(sh)+50;privateStatus(-8);return;}
 for(unsigned i=0;i<n;++i){const unsigned slot=(unsigned(tail)+i)&16383;memcpy((void*)(uintptr_t(0x8B4204+slot*111)),&wire.local[i],111);put<uint32_t>(0xA70204+slot*4,timing);}
 put<int>(0x8B4200,(tail+int(n))&16383);put<int>(0x8B41F8,count+int(n));
 if(network){if(protocol)put<uint16_t>(0xAFA400,uint16_t(serial+n));put<uint32_t>(0xB0487C,get<uint32_t>(0xB0487C)+1);
  if(protocol&&wire.mega)for(unsigned i=0;i<8;++i)put<uint32_t>(0xAFA338+i*4,0);
 }
 privateQueue.pop(n);privateRetry=0;privateDispatchBudget-=int(n);sh->privateOutSent+=n;privateStatus(1);
 if(network)call<void>(0x541820,transport);
}
static void servicePrivateOut(){
 const int frame=get<int>(0xA8ED84);if(frame!=privateDispatchFrame){privateDispatchFrame=frame;privateDispatchBudget=128;privatePruneBudget=128;}
 // Match native OutList capacity per frame; manual commands do not consume
 // automation admission credit. At most six fixed 24-event packet batches.
 for(unsigned i=0;i<6&&privateQueue.count&&privateDispatchBudget>0;++i){const unsigned before=privateQueue.count;servicePrivateOutBatch();if(privateQueue.count==before)break;}
}
struct PrivateReturn {uint32_t original=0,packet=0,stack=0;const privateout::CaptureSite*site=nullptr;};
static PrivateReturn privateReturns[16];static unsigned privateReturnDepth=0;
static void privatePostEntry();static void privateMegaDone();
// PUSHAD frame plus flags, saved destination ESP, and jump cell. POPAD ignores
// its ESP field; post-return uses the separate destination after restoring FP.
struct PrivateFrame {uint32_t edi,esi,ebp,esp,ebx,edx,ecx,eax,flags,nextStack,jump;};
static void privateInstallReturn(uint32_t sp,uint32_t packet,uint32_t ctor,unsigned bytes){
 if(privateReturnDepth==16)return;auto*slot=(uint32_t*)(uintptr_t)sp;
 const auto*s=privateout::captureSite(*slot);if(!s||s->constructor!=ctor)return;
 if(s->caller==0x6FFE39&&get<uint8_t>(0xAC4CF4))return; // native local planning first
 privateReturns[privateReturnDepth++]={*slot,packet,sp+4+bytes,s};*slot=uint32_t(uintptr_t(&privatePostEntry));
}
static int privateOutAction(unsigned op,uint32_t frameAddress,uint32_t source){
 auto&f=*(PrivateFrame*)(uintptr_t)frameAddress;
 if(!privateOutActive()||GetCurrentThreadId()!=privateOwner)return 0;
 if(op==15){
  const uint32_t sp=frameAddress+44,ret=*(uint32_t*)(uintptr_t)sp;
  if(source==1){privateInstallReturn(sp,f.ecx,0x4C6860,40);return 1;}
  uint32_t ctor=0;unsigned args=0,offset=20;
  switch(ret){
  case 0x4C6601:ctor=0x4C65E0;args=16;break;case 0x4C6671:ctor=0x4C6650;args=12;break;
  case 0x4C66E1:ctor=0x4C66C0;args=8;break;case 0x4C6741:ctor=0x4C6720;args=12;break;
  case 0x4C67A1:ctor=0x4C6780;args=24;break;case 0x4C68ED:ctor=0x4C68E0;args=40;offset=12;break;
  case 0x4C6991:ctor=0x4C6970;args=20;break;case 0x4C6B01:ctor=0x4C6AE0;args=24;break;
  case 0x4C6B81:ctor=0x4C6B60;args=16;break;default:return 0;
  }
  privateInstallReturn(sp+offset,f.esi,ctor,args);return 1;
 }
 if(op==17){
  const Event&e=*(Event*)(uintptr_t)(frameAddress+48);
  if(privateout::payloadBytes(e.kind)<0)return 0;
  f.eax=0;f.jump=1;f.eax=privateOutQueue(e,true)?1:0;return 1;
 }
 if(op!=16)return 0;
 const auto&r=*(PrivateReturn*)(uintptr_t)source;const auto&s=*r.site;
 const Event&e=*(Event*)(uintptr_t)r.packet;
 if(privateout::payloadBytes(e.kind)<0)return 0;
 f.jump=s.failure;if(s.result)f.eax=0;
 const bool accepted=privateOutQueue(e,true);
 if(!accepted){f.jump=s.failure;if(s.result)f.eax=0;return 1;}
 f.jump=s.caller==0x646EF5?uint32_t(uintptr_t(&privateMegaDone)):s.success;
 const uint32_t sp=frameAddress+44;
 if(s.esi>=0)f.esi=*(uint32_t*)(uintptr_t)(sp+s.esi);
 if(s.edi>=0)f.edi=*(uint32_t*)(uintptr_t)(sp+s.edi);
 if(s.ebp>=0)f.ebp=*(uint32_t*)(uintptr_t)(sp+s.ebp);
 f.nextStack+=s.stackBytes;if(s.result)f.eax=1;
 return 1;
}
// Pop and initialize the continuation OUTSIDE the fallible guarded callback.
// Thus a declined capture cannot strand a native constructor's return address.
static void __cdecl privatePostDispatch(PrivateFrame*f){
 const uint32_t sp=uint32_t(uintptr_t(f))+44;
 if(!privateReturnDepth||privateReturns[privateReturnDepth-1].stack!=sp)TerminateProcess(GetCurrentProcess(),0xE0000015);
 const auto r=privateReturns[--privateReturnDepth];f->jump=r.original;f->nextStack=sp;
 GuardedRiskAction(16,uint32_t(uintptr_t(f)),uint32_t(uintptr_t(&r)));
 *(uint32_t*)(uintptr_t)(f->nextStack-4)=f->jump;
}

static __declspec(naked) void privateLogEntry(){__asm {
 pushfd
 cmp dword ptr [esp+4],4C6601h
 jb log_pass
 cmp dword ptr [esp+4],4C6B81h
 ja log_pass
 popfd
 push 0
 push 0
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
 push ebx
 push 15
 call GuardedRiskAction
 add esp,12
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 lea esp,[esp+8]
 ret
log_pass:
 popfd
 ret
}}

static __declspec(naked) void privateMegaEntry(){__asm {
 push 0
 push 0
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
 push ebx
 push 15
 call GuardedRiskAction
 add esp,12
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 lea esp,[esp+8]
 mov eax,ecx
 mov ecx,[esp+4]
 push 4C6866h
 ret
}}

static __declspec(naked) void privateByValueEntry(){__asm {
 push 0
 push 0
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
 push ebx
 push 17
 call GuardedRiskAction
 add esp,12
 cmp dword ptr [ebx+40],1
 je queue_done
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 lea esp,[esp+8]
 mov eax,ds:[0A8ED84h]
 push 6521C5h
 ret
queue_done:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 lea esp,[esp+8]
 ret 70h
}}

static __declspec(naked) void privatePostEntry(){__asm {
 push 0
 push 0
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
 push ebx
 call privatePostDispatch
 add esp,4
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov esp,[esp]
 lea esp,[esp-4]
 ret
}}
static __declspec(naked) void privateMegaDone(){__asm {
 mov al,1
 pop edi
 pop esi
 add esp,70h
 ret 0Ch
}}
static RiskSite privateSites[]={
 {0x4068E0,5,{0xC3,0x90,0x90,0x90,0x90},{}},
 {0x4C6860,6,{0x8B,0xC1,0x8B,0x4C,0x24,0x04},{}},
 {0x6521C0,5,{0xA1,0x84,0xED,0xA8,0},{}},
};
static uint32_t privateEntry(unsigned tag){
 return tag==7?uint32_t(uintptr_t(&privateLogEntry)):tag==8?uint32_t(uintptr_t(&privateMegaEntry)):tag==9?uint32_t(uintptr_t(&privateByValueEntry)):0;
}
static bool privateCaptureCompatible(){
 for(const auto&p:privateout::CaptureProbes){uint8_t b[8]{};if(!read(p.at,b,p.size)||memcmp(b,p.bytes,p.size))return false;}
 return true;
}
static bool privateOutRetired(){
 if(privateEnabled||privateReturnDepth||!privateBpRetired())return false;
 for(const auto&s:privateSites)if(codeHookRegistry.active(s.address))return false;return true;
}
static void restorePrivateOut(){
 privateEnabled=false;privateBpRequest(false);
 if(sh)sh->privateOutCancelled+=privateQueue.count;privateQueue.clear();privateRetry=0;privateDispatchFrame=-1;privateDispatchBudget=0;
 bool ok=true;for(auto&s:privateSites)if(!restoreRiskSite(s))ok=false;
 if(ok&&privateBpRetired()&&!privateReturnDepth){privateMode=0;privateOwner=0;privateStatus(0);}else privateStatus(-4);
}
static void privateOutTick(){
 const uint32_t requested=sh?sh->privateOutMode:0;
 if(!requested||requested>2||!sh||sh->stop||fault||!readonly::commands(sh)||readOnlyState.active||!riskReady()){restorePrivateOut();return;}
 if(privateMode&&privateMode!=requested){restorePrivateOut();if(!privateOutRetired())return;}
 const DWORD now=sampleClock(sh);
 const bool audit=privateEpoch!=match||!privateChecked||DWORD(now-privateChecked)>=1000;
 if(!audit&&privateMode==requested&&privateEnabled)return;
 if(audit){privateChecked=now;privateEpoch=match;privateCompatible=privateCaptureCompatible();}
 if(!privateCompatible){restorePrivateOut();privateStatus(-1);return;}
 if(!privateMode&&directSellPending()){privateStatus(2);return;}
 privateMode=requested;privateOwner=GetCurrentThreadId();
 if(requested==privateout::Hook){
  if(!privateBpRequest(false)||!privateBpRetired()){privateEnabled=false;privateStatus(-4);return;}
  bool ok=true;
  for(unsigned i=0;i<3;++i){auto&s=privateSites[i];if(!s.after[0]){memset(s.after,0x90,s.size);s.after[0]=0xE9;const uint32_t offset=privateEntry(i+7)-s.address-5;memcpy(s.after+1,&offset,4);}if(!installRiskSite(s)){ok=false;break;}}
  if(!ok){privateEnabled=false;for(auto&s:privateSites)restoreRiskSite(s);privateStatus(-3);return;}
  privateEnabled=true;
 }else{
  for(auto&s:privateSites)if(!restoreRiskSite(s)){privateEnabled=false;privateStatus(-4);return;}
  privateEnabled=privateBpRequest(true);if(!privateEnabled){privateStatus(privateBpStatus());return;}
 }
 if(!privateQueue.count)privateStatus(get<int>(0xA802C8)?2:1);
}
