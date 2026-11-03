#include "tech_ui_breakpoint_core.h"
#include "tech_enhancement_parameter_policy.h"
extern "C" int __cdecl GuardedTechUiParameter(void*,unsigned);
extern "C" int __cdecl GuardedTechEnhancementProbe(unsigned,unsigned);
extern "C" unsigned BeginTechEnhancementProbe();extern "C" void EndTechEnhancementProbe(unsigned);
extern "C" unsigned techEnhanceProbeReturn=0;
extern "C" __declspec(naked) int __stdcall AssistTechEnhancementProbeCall(unsigned,unsigned){__asm {
 mov ecx,[esp+4]
 mov eax,[esp+8]
 push 1
 push 0
 push eax
 mov eax,4F7870h
 call eax
 ret 8
}}
namespace techparameters {
struct TypeIdentity{uint32_t pointer=0,id=0,table=0;};
struct EnhancedIdentity{TypeIdentity type;uint32_t items=0;int index=-1,level=0;};
struct Policy{volatile LONG enabled=0;uint32_t house=0,houseID=0,scenario=0;int session=-1,frame=-1;TypeIdentity types[1024]{};volatile LONG enhanced=0;EnhancedIdentity enhancement[techenhancement::Count]{};};
static Policy policy;static techbp::Service service;
static int failure=0,enhancementFailure=0;static bool desired=false,enhancementDesired=false;
static DWORD auditAt=0,retryAt=0,enhancementParameterScan=0;static uint32_t attempted=0,refusedGroups=0,resourceGroups=0;
static volatile LONG fullExceptions=0,enhancementExceptions=0;static unsigned probeDepth=0;static uint32_t probeToken=0;
static constexpr uint32_t sites[4]={0x6A97D2,0x6AA781,0x4F7951,0x4F7B66};
inline unsigned hash(uint32_t p){return ((p>>4)*2654435761u)&1023u;}
static bool signature(uint32_t at,unsigned n,uint32_t expected){uint8_t bytes[32]{};if(n>sizeof bytes||!read(at,bytes,n))return false;uint32_t h=2166136261u;for(unsigned i=0;i<n;++i)h=(h^bytes[i])*16777619u;return h==expected;}
static bool nativeCompatible(){return signature(0x6A97C3,20,0xD3C40F9Du)&&signature(0x6AA777,15,0x26F9619Eu)&&signature(0x4F7870,16,0x6995B50Eu);}
static bool enhancementCompatible(){return signature(0x4F794A,13,0x0FC7BB79u)&&signature(0x4F7B5C,16,0x15DAD6B8u)&&signature(0x4F7870,16,0x6995B50Eu);}
static bool enhancedRequested(){return sh&&sh->techEnhance&&sh->techEnhanceImplementation==techenhanceparams::Exact&&!sh->stop&&!fault;}
static bool healthy(){return service.state!=techbp::System&&service.state!=techbp::Foreign&&service.state!=techbp::Thread;}
static bool ready(uint32_t group){return healthy()&&(service.activeTags()&group)==group;}
static bool fullType(uint32_t pointer){unsigned n=hash(pointer);for(unsigned probe=0;probe<1024;++probe,n=(n+1)&1023u){const auto&t=policy.types[n];if(!t.pointer)return false;if(t.pointer==pointer)return get<uint32_t>(pointer)==t.table&&get<uint32_t>(pointer+0x10)==t.id;}return false;}
// Diagnostics follow the feature owning outstanding cleanup. A healthy
// sibling may keep the shared handler alive after this group has retired.
static bool groupOff(uint32_t group){
 if((service.lease.tags()&group)||service.lease.uncertain||service.pendingFor(group))return false;
 return (service.lease.tags()&(15u&~group))||!(resourceGroups&group)||!service.resources();
}
static void publish(){
 if(!sh)return;const uint32_t active=service.activeTags();
 sh->techParameterStatus=failure?failure:desired?(ready(3)?techbp::Ready:(refusedGroups&3)?techbp::Slots:service.state<0?service.state:techbp::Waiting):groupOff(3)?techbp::Off:service.state<0?service.state:techbp::Waiting;
 sh->techParameterError=uint32_t(service.lastError);sh->techParameterSlots=__builtin_popcount(active&3);sh->techParameterExceptions=uint32_t(fullExceptions);
 sh->techEnhanceParameterStatus=enhancementFailure?enhancementFailure:enhancementDesired?(ready(12)?techbp::Ready:(refusedGroups&12)?techbp::Slots:service.state<0?service.state:techbp::Waiting):groupOff(12)?techbp::Off:service.state<0?service.state:techbp::Waiting;
 sh->techEnhanceParameterError=uint32_t(service.lastError);sh->techEnhanceParameterSlots=__builtin_popcount(active&12);sh->techEnhanceParameterExceptions=uint32_t(enhancementExceptions);
}
static bool callback(void*context,uint32_t tag){return GuardedTechUiParameter(context,tag)!=0;}
static bool captureWorld(uint32_t house){
 if(!sh||sh->stop||fault||house!=lastHouse||!house||get<uint32_t>(0xA83D4C)!=house)return false;
 uint32_t scenario=0,id=0;int frame=-1,session=-1;
 if(!read(0xA8B230,&scenario,4)||!scenario||!read(0xA8ED84,&frame,4)||frame<0||!read(0xA8B238,&session,4)||!eventdelivery::mayQueue(session)||!read(house+0x10,&id,4)||!id)return false;
 if(policy.house!=house||policy.houseID!=id||policy.scenario!=scenario||policy.session!=session||frame<policy.frame){InterlockedExchange(&policy.enabled,0);InterlockedExchange(&policy.enhanced,0);}
 policy.house=house;policy.houseID=id;policy.scenario=scenario;policy.frame=frame;policy.session=session;return true;
}
static bool capture(uint32_t house){
 InterlockedExchange(&policy.enabled,0);if(!captureWorld(house))return false;
 for(auto&t:policy.types)t={};for(int i=0;i<snap.typeCount&&i<MaxTypes;++i)if(techAllowed[i]&&typePointers[i]){
  uint32_t words[5]{};if(!read(typePointers[i],words,sizeof words)||!words[0]||!words[4])return false;
  unsigned n=hash(typePointers[i]);while(policy.types[n].pointer&&policy.types[n].pointer!=typePointers[i])n=(n+1)&1023u;
  policy.types[n]={typePointers[i],words[4],words[0]};
 }MemoryBarrier();return true;
}
static bool world(){
 return sh&&!sh->stop&&!fault&&get<uint32_t>(0xA83D4C)==policy.house&&get<uint32_t>(policy.house+0x10)==policy.houseID&&get<uint32_t>(0xA8B230)==policy.scenario&&
  get<int>(0xA8B238)==policy.session&&get<int>(0xA8ED84)>=policy.frame&&get<bool>(0xA8E9A0)&&policy.house!=get<uint32_t>(0xAC1198)&&!get<bool>(policy.house+0x1F5)&&!get<bool>(policy.house+0x1F6)&&!get<bool>(policy.house+0x1F8);
}
static bool configure(){
 const uint32_t wanted=(desired&&!failure?3u:0u)|(enhancementDesired&&(!enhancementFailure||enhancementFailure==-7)?12u:0u);service.poll();if(!service.resources())resourceGroups=0;
 if(!service.pending()){refusedGroups&=~service.activeTags();if(service.state==techbp::Slots)refusedGroups|=attempted&~service.activeTags();}
 if(service.pending()){publish();return false;}
 if(!healthy()){
  InterlockedExchange(&policy.enabled,0);InterlockedExchange(&policy.enhanced,0);
  if(desired)failure=int(service.state);if(enhancementDesired)enhancementFailure=int(service.state);service.retire();publish();return false;
 }
 const DWORD now=sh?sampleClock(sh):GetTickCount();
 if(!wanted){service.retire();attempted=0;retryAt=0;refusedGroups=0;}
 else if(service.activeTags()!=wanted){
  if(attempted!=wanted||!retryAt||DWORD(now-retryAt)>=500){attempted=wanted;retryAt=now;resourceGroups|=wanted;service.configure(sites,wanted,callback);}
 }else if(!auditAt||DWORD(now-auditAt)>=500){auditAt=now;service.audit();}
 // Slot exhaustion leaves the verified existing subset untouched.
 if(desired&&!failure)InterlockedExchange(&policy.enabled,ready(3)?1:0);
 if(enhancementDesired&&(!enhancementFailure||enhancementFailure==-7))InterlockedExchange(&policy.enhanced,ready(12)?1:0);
 publish();return service.activeTags()==wanted;
}
}
extern "C" int __cdecl AssistTechUiParameter(void*raw,unsigned tag){
 using namespace techparameters;auto&context=*static_cast<CONTEXT*>(raw);
 if(tag>=4||context.Eip!=sites[tag])return 0;
 if(tag<2)InterlockedIncrement(&fullExceptions);else InterlockedIncrement(&enhancementExceptions);
 if(!world())return 0;
 if(tag<2){
  if(!policy.enabled||!(sh->flags&UnsafeTech)||!techmode::parameters(techImplementation)||context.Ecx!=policy.house)return 0;
  if(!signature(tag==0?0x6A97C3:0x6AA777,tag==0?20:15,tag==0?0xD3C40F9Du:0x26F9619Eu)){InterlockedExchange(&policy.enabled,0);return 0;}
  auto*args=reinterpret_cast<uint32_t*>(uintptr_t(context.Esp));const uint32_t pointer=args[0];
  if(pointer<0x10000||args[1]||args[2]!=(tag==0?0u:1u)||!fullType(pointer))return 0;args[1]=1;return 1;
 }
 if(!policy.enhanced||!sh->techEnhance||!enhancementCompatible())return 0;
 const uint32_t stack=context.Esp,returnAddress=get<uint32_t>(stack+0x4C),pointer=get<uint32_t>(stack+0x50);
 if(context.Ebp!=policy.house||get<uint32_t>(stack+0x10)!=policy.house||get<uint32_t>(stack+0x54)||
  (tag==2?context.Edi:context.Ecx)!=pointer||pointer<0x10000)return 0;
 const uint32_t include=get<uint32_t>(stack+0x58);const bool caller=(returnAddress==0x6A97D7&&include==0)||(returnAddress==0x6AA786&&include==1)||
  (probeDepth&&returnAddress==techEnhanceProbeReturn&&include==1&&probeToken&&uint32_t(InterlockedCompareExchange(&gameentry::activeToken,0,0))==probeToken);
 if(!caller)return 0;
 for(unsigned i=0;i<techenhancement::Count;++i){const auto&t=policy.enhancement[i];if(t.type.pointer!=pointer)continue;
  if(get<uint32_t>(pointer)!=t.type.table||get<uint32_t>(pointer+0x10)!=t.type.id||get<int>(pointer+0x634)!=t.level||
     get<uint32_t>(techenhancement::Rules[i].array+4)!=t.items||t.index<0||t.index>=get<int>(techenhancement::Rules[i].array+16)||get<uint32_t>(t.items+uint32_t(t.index)*4)!=pointer)return 0;
  context.EFlags=techenhanceparams::compareFlags(1,tag==2?UINT32_MAX:context.Ebx,context.EFlags);return 1;
 }return 0;
}
extern "C" unsigned BeginTechEnhancementProbe(){using namespace techparameters;if(!gameentry::active(gameentry::Client::Assist)||!policy.enhanced||!ready(12)||probeDepth>=16)return 0;static constexpr uint8_t stub[]={0x8B,0x4C,0x24,4,0x8B,0x44,0x24,8,0x6A,1,0x6A,0,0x50,0xB8,0x70,0x78,0x4F,0,0xFF,0xD0,0xC2,8,0};
 const auto*start=reinterpret_cast<const uint8_t*>(&AssistTechEnhancementProbeCall);if(memcmp(start,stub,sizeof stub))return 0;
 techEnhanceProbeReturn=uint32_t(uintptr_t(start+20));if(!probeDepth)probeToken=uint32_t(InterlockedCompareExchange(&gameentry::activeToken,0,0));return ++probeDepth;}
extern "C" void EndTechEnhancementProbe(unsigned token){using namespace techparameters;if(token&&token==probeDepth)--probeDepth;else probeDepth=0;if(!probeDepth)probeToken=0;}
static bool techParametersRetired(){using namespace techparameters;return !(service.lease.mask&&((service.lease.tags()&3)||service.lease.uncertain))&&!service.pending()&&((service.lease.tags()&12)||!service.resources());}
static bool techEnhanceParametersRetired(){using namespace techparameters;return !(service.lease.mask&&((service.lease.tags()&12)||service.lease.uncertain))&&!service.pending()&&((service.lease.tags()&3)||!service.resources());}
static bool restoreTechParameters(){using namespace techparameters;const bool changed=desired||(service.lease.tags()&3)||service.lease.uncertain;InterlockedExchange(&policy.enabled,0);desired=false;failure=0;if(changed)attempted=0;refusedGroups&=~3u;configure();return techParametersRetired();}
static bool restoreTechEnhanceParameters(){using namespace techparameters;const bool changed=enhancementDesired||(service.lease.tags()&12)||service.lease.uncertain;InterlockedExchange(&policy.enhanced,0);enhancementDesired=false;enhancementFailure=0;if(changed)attempted=0;refusedGroups&=~12u;configure();return techEnhanceParametersRetired();}
static void maintainTechParameters(){
 using namespace techparameters;service.poll();
 if(!sh||sh->stop||fault){InterlockedExchange(&policy.enabled,0);InterlockedExchange(&policy.enhanced,0);desired=enhancementDesired=false;failure=enhancementFailure=0;}
 else if(!(sh->flags&UnsafeTech)||!techmode::parameters(techImplementation)){InterlockedExchange(&policy.enabled,0);desired=false;failure=0;}
 configure();
}
static bool requestTechParameters(uint32_t house){
 using namespace techparameters;maintainTechParameters();if(failure){publish();return false;}
 if(!nativeCompatible()){failure=techbp::Signature;InterlockedExchange(&policy.enabled,0);desired=false;configure();return false;}
 if(!restoreTechEnhancement()||!capture(house)){publish();return false;}desired=true;configure();return ready(3)&&!failure;
}
static int techEnhancementCatalogueLevel(uint32_t type,int level){
 if(!techparameters::enhancedRequested()||!type||(!techenhancement::eligible(techenhancement::Rules[9],level)&&level!=-1))return level;
 uint32_t table=0;char id[24]{};if(!read(type,&table,4)||!read(type+0x24,id,sizeof id)||!memchr(id,0,sizeof id))return level;
 for(const auto&r:techenhancement::Rules)if(table==r.vtable&&techenhancement::sameID(id,r.id)&&techenhancement::eligible(r,level))return 1;return level;
}
static bool techEnhancementProductionPending(uint32_t house){
 const unsigned offsets[]={0x53AC,0x53B0,0x53B4,0x53B8,0x53BC,0x53CC};
 auto dependent=[&](uint32_t type){return !type||(!(techparameters::desired&&techparameters::ready(3)&&techparameters::fullType(type))&&call<int>(0x4F7870,house,(void*)(uintptr_t)type,false,true)<=0);};
 for(unsigned offset:offsets){uint32_t factory=0,product=0,items=0,owner=0;int count=0;if(!read(house+offset,&factory,4))return true;if(!factory)continue;
  if(factory<0x10000||!read(factory+0x6C,&owner,4)||owner!=house||!read(factory+0x58,&product,4)||!read(factory+0x44,&items,4)||!read(factory+0x50,&count,4)||count<0||count>128)return true;
  if(product&&(product<0x10000||dependent(typeOf(product,kind(product)))))return true;
  if(count&&(items<0x10000||uint64_t(items)+uint64_t(count)*4>UINT32_MAX))return true;
  for(int i=0;i<count;++i){uint32_t type=0;if(!read(items+uint32_t(i)*4,&type,4)||dependent(type))return true;}
 }return false;
}
static void deferTechEnhancementParameters(){
 restoreTechEnhanceParameters();techparameters::enhancementFailure=-7;techparameters::publish();
}
static void techEnhanceParametersTick(uint32_t house,DWORD now){
 using namespace techparameters;
 if(!enhancedRequested()){
  if(enhancementDesired&&sh&&sh->techEnhance&&techEnhancementProductionPending(house)){enhancementFailure=-7;publish();return;}
  restoreTechEnhanceParameters();return;
 }
 if(techLegacyEnhancementProductionPending(house)){deferTechEnhancementParameters();return;}
 if(((snap.techOpen||techPending)&&!techmode::parameters(techImplementation))||techDataPending()||!nativeTextTechRetired()||!restoreTechEnhancement()){
  restoreTechEnhanceParameters();enhancementFailure=-6;publish();return;
 }
 if(!enhancementCompatible()){restoreTechEnhanceParameters();enhancementFailure=techbp::Signature;publish();return;}
 if(!captureWorld(house)){InterlockedExchange(&policy.enhanced,0);enhancementFailure=techbp::Thread;publish();return;}
 enhancementFailure=0;enhancementDesired=true;
 if(!enhancementParameterScan||DWORD(now-enhancementParameterScan)>=500||!policy.enhanced){
  enhancementParameterScan=now;InterlockedExchange(&policy.enhanced,0);for(auto&t:policy.enhancement)t={};
  for(unsigned i=0;i<techenhancement::Count;++i){const auto&r=techenhancement::Rules[i];Vec types=vec(r.array,4096);
   for(int index=0;index<types.count;++index){const uint32_t pointer=item(types,index);if(!pointer||techEnhancementCatalogueLevel(pointer,get<int>(pointer+0x634))!=1)continue;
    char id[24]{};if(get<uint32_t>(pointer)!=r.vtable||!read(pointer+0x24,id,sizeof id)||!memchr(id,0,sizeof id)||!techenhancement::sameID(id,r.id)||!techenhancement::eligible(r,get<int>(pointer+0x634)))continue;
    const uint32_t uid=get<uint32_t>(pointer+0x10);if(!uid)continue;
    policy.enhancement[i]={{pointer,uid,r.vtable},types.items,index,get<int>(pointer+0x634)};break;
   }
  }
 }
 configure();if(!ready(12))return;
 sidebarpolicy::CameoSet icons;if(!techCameoInventory(icons))return;
 for(const auto&r:policy.enhancement){const uint32_t type=r.type.pointer;if(!type)continue;const int i=typeSlot(type);
  if(i<0||icons.contains(snap.types[i].kind,snap.types[i].index)||GuardedTechEnhancementProbe(house,type)<=0)continue;
  uint32_t factory=techFactory(type,house);if(!factory)continue;const auto&t=snap.types[i];const int tab=typeTab(i);
  if(tab<0||tab>3||get<int>(Tabs+tab*3988+84)>=75)continue;
  if(call<bool>(0x6A6300,Sidebar,t.kind,t.index)){sidebarRedraw(tab);icons.add(t.kind,t.index);}
 }
}
