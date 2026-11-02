#include "tech_ui_breakpoint_core.h"
extern "C" int __cdecl GuardedTechUiParameter(void*,unsigned);
namespace techparameters {
struct TypeIdentity{uint32_t pointer=0,id=0,table=0;};
struct Policy{volatile LONG enabled=0;uint32_t house=0,houseID=0,scenario=0;int session=-1,frame=-1;TypeIdentity types[1024]{};};
static Policy policy;
static techbp::Service service;
static int failure=0;static bool desired=false;static DWORD auditAt=0;
inline unsigned hash(uint32_t p){return ((p>>4)*2654435761u)&1023u;}
static bool signature(uint32_t at,unsigned n,uint32_t expected){uint8_t bytes[32]{};if(n>sizeof bytes||!read(at,bytes,n))return false;uint32_t h=2166136261u;for(unsigned i=0;i<n;++i)h=(h^bytes[i])*16777619u;return h==expected;}
static bool nativeCompatible(){
 // Full pre-CALL context includes the three native argument pushes; neither
 // address is an IAT/vtable call. Exact instruction stream from supported PE.
 return signature(0x6A97C3,20,0xD3C40F9Du)&&signature(0x6AA777,15,0x26F9619Eu)&&
  signature(0x4F7870,16,0x6995B50Eu);
}
static void publish(){if(!sh)return;sh->techParameterStatus=failure?failure:service.state;sh->techParameterError=uint32_t(service.lastError);sh->techParameterSlots=__builtin_popcount(service.lease.mask);sh->techParameterExceptions=uint32_t(service.exceptions);}
static bool callback(void*context,uint32_t tag){return GuardedTechUiParameter(context,tag)!=0;}
static bool capture(uint32_t house){
 InterlockedExchange(&policy.enabled,0);
 if(!sh||sh->stop||fault||house!=lastHouse||!house||get<uint32_t>(0xA83D4C)!=house)return false;
 uint32_t scenario=0;int frame=-1,session=-1;
 if(!read(0xA8B230,&scenario,4)||!scenario||!read(0xA8ED84,&frame,4)||frame<0||
    !read(0xA8B238,&session,4)||!eventdelivery::mayQueue(session))return false;
 policy.house=house;if(!read(house+0x10,&policy.houseID,4)||!policy.houseID)return false;policy.scenario=scenario;policy.frame=frame;policy.session=session;
 for(auto&t:policy.types)t={};
 for(int i=0;i<snap.typeCount&&i<MaxTypes;++i)if(techAllowed[i]&&typePointers[i]){
  uint32_t words[5]{};if(!read(typePointers[i],words,sizeof words)||!words[0]||!words[4])return false;
  unsigned n=hash(typePointers[i]);while(policy.types[n].pointer&&policy.types[n].pointer!=typePointers[i])n=(n+1)&1023u;
  policy.types[n]={typePointers[i],words[4],words[0]};
 }
 MemoryBarrier();InterlockedExchange(&policy.enabled,1);return true;
}
}
// This body performs bounded private-table/code/identity reads and changes
// only one existing call's stack bool. Its SEH wrapper never calls the engine,
// allocates, logs, waits or acquires the game dispatcher.
extern "C" int __cdecl AssistTechUiParameter(void*raw,unsigned tag){
 using namespace techparameters;auto&context=*static_cast<CONTEXT*>(raw);
 const uint32_t site=tag==0?0x6A97D2:tag==1?0x6AA781:0;
 if(!site||context.Eip!=site||!policy.enabled||!sh||sh->stop||fault||!(sh->flags&UnsafeTech)||!techmode::parameters(techImplementation)||context.Ecx!=policy.house||
    get<uint32_t>(0xA83D4C)!=policy.house||get<uint32_t>(policy.house+0x10)!=policy.houseID||get<uint32_t>(0xA8B230)!=policy.scenario||
    get<int>(0xA8B238)!=policy.session||get<int>(0xA8ED84)<policy.frame||!get<bool>(0xA8E9A0)||policy.house==get<uint32_t>(0xAC1198)||get<bool>(policy.house+0x1F5)||get<bool>(policy.house+0x1F6)||get<bool>(policy.house+0x1F8))return 0;
 const auto*code=reinterpret_cast<const uint8_t*>(uintptr_t(tag==0?0x6A97C3:0x6AA777));uint32_t h=2166136261u;
 for(unsigned i=0;i<(tag==0?20u:15u);++i)h=(h^code[i])*16777619u;
 if(h!=(tag==0?0xD3C40F9Du:0x26F9619Eu)){InterlockedExchange(&policy.enabled,0);return 0;}
 auto*args=reinterpret_cast<uint32_t*>(uintptr_t(context.Esp));const uint32_t pointer=args[0];
 if(pointer<0x10000||args[1]||args[2]!=(tag==0?0u:1u))return 0;
 unsigned n=hash(pointer);
 for(unsigned probe=0;probe<1024;++probe,n=(n+1)&1023u){
  const auto&t=policy.types[n];if(!t.pointer)return 0;if(t.pointer!=pointer)continue;
  if(get<uint32_t>(pointer)!=t.table||get<uint32_t>(pointer+0x10)!=t.id)return 0;
  args[1]=1;return 1;
 }return 0;
}
static bool techParametersRetired(){return !techparameters::service.resources();}
static bool restoreTechParameters(){
 using namespace techparameters;InterlockedExchange(&policy.enabled,0);desired=false;
 bool done=service.retire();if(done){failure=0;auditAt=0;}publish();return done;
}
static void maintainTechParameters(){
 using namespace techparameters;service.poll();
 if(!sh||sh->stop||fault||!(sh->flags&UnsafeTech)||techImplementation!=techmode::Parameters){restoreTechParameters();return;}
 if(service.state<0){failure=int(service.state);InterlockedExchange(&policy.enabled,0);service.retire();}
 if(!failure&&service.state==techbp::Ready&&!service.pending()){
  DWORD now=sampleClock(sh);if(!auditAt||DWORD(now-auditAt)>=500){auditAt=now;service.audit();}
 }
 publish();
}
static bool requestTechParameters(uint32_t house){
 using namespace techparameters;maintainTechParameters();if(failure){publish();return false;}
 if(!nativeCompatible()){failure=techbp::Signature;InterlockedExchange(&policy.enabled,0);service.retire();publish();return false;}
 if(!restoreTechEnhancement()||!capture(house)){publish();return false;}
 if(!desired){desired=true;service.enable(0x6A97D2,0x6AA781,callback);}
 maintainTechParameters();return !failure&&service.state==techbp::Ready&&__builtin_popcount(service.lease.mask)==2;
}
