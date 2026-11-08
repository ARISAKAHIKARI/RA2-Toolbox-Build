// Production bodies are extracted verbatim by control_publication.py.
// Service/clock, game memory and the portable exception boundary are models;
// actual Windows SEH is separately compiled by exception_policy.py.
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>
#include <vector>
#include "../source/control_parameter_policy.h"
#include "../source/control_mode_policy.h"
#include "../source/tech_ui_breakpoint_hot.h"
#include "../source/tech_parameter_lookup.h"
#include "../source/tech_enhancement.h"
#include "../source/tech_enhancement_parameter_policy.h"
#include "../source/technology_mode_policy.h"
#include "../source/map_visibility_parameter_policy.h"
#include "../source/single_player_fps.h"
#include "../source/event_delivery.h"
#include "../source/exception_policy.h"
#define __cdecl
using LONG=int32_t;using DWORD=uint32_t;
using namespace assist;
static unsigned checks=0,hotCalls=0,coldCalls=0,transitions=0;
static void check(bool value,const char*what){++checks;if(!value){std::fprintf(stderr,"check %u failed: %s\n",checks,what);std::abort();}}
static constexpr uint32_t House=0x10000,Target=0x20000,Stack=0x40000,Scenario=0x60000;
using Trace=std::vector<std::pair<uint32_t,size_t>>;
static std::map<uint32_t,uint8_t> memory;static Trace trace;
static uint32_t clockValue=1000,lastError=0,throwAt=0,throwCode=0,throwFlags=0;
static void(*readObserver)()=nullptr;
struct MemoryFault{uint32_t code,flags;};
static DWORD GetTickCount(){return clockValue;}
static LONG InterlockedExchange(volatile LONG*p,LONG next){const LONG old=*p;*p=next;return old;}
template<class T>static void put(uint32_t address,T value){const auto*p=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)memory[address+uint32_t(i)]=p[i];}
static bool read(uint32_t address,void*out,size_t size){for(size_t i=0;i<size;++i){auto it=memory.find(address+uint32_t(i));if(it==memory.end())return false;static_cast<uint8_t*>(out)[i]=it->second;}return true;}
template<class T>static T get(uint32_t address){T value{};read(address,&value,sizeof value);return value;}
static bool localRead(uint32_t address,void*out,size_t size){
 trace.emplace_back(address,size);if(readObserver)readObserver();
 if(address<0x10000||uint64_t(address)+size>=0x80000000ull||(size!=4&&size!=1))return false;
 if(throwAt>=address&&uint64_t(throwAt)<uint64_t(address)+size){lastError=0xBAD;throw MemoryFault{throwCode,throwFlags};}
 if(!read(address,out,size)){lastError=0xBAD;throw MemoryFault{0xc0000005u,0};}return true;
}
template<class Callback>static int modeledNoReport(Callback callback){
 struct Restore{uint32_t saved;~Restore(){lastError=saved;}}restore{lastError};
 try{return callback();}catch(const MemoryFault&fault){
  assistguard::Record record{};record.code=fault.code;record.flags=fault.flags;assistguard::Pointers pointers{&record,nullptr};
  if(assistguard::noReportAccess(&pointers))return 0;throw;
 }
}
namespace assist {namespace techbp {
inline constexpr unsigned SiteCount=6,AllTags=63;
enum Status{Off=0,Ready=1,Waiting=2,Signature=-1,Slots=-2,System=-3,Foreign=-4,Thread=-5};
// Explicit seam: this models confirmed slots and pending owner transactions,
// not OS contexts, VEH, registration or execution of an engine instruction.
struct Service{
 struct Lease{uint32_t active=0;bool uncertain=false;uint32_t tags()const{return active;}}lease;
 int state=Ready;bool inTransaction=false,allocated=false;uint32_t allowedTags=AllTags;
 unsigned polls=0,configures=0,retires=0,audits=0,inspections=0;
 uint32_t activeTags()const{return lease.active;}bool pending()const{return inTransaction;}
 bool pendingFor(uint32_t)const{return inTransaction;}bool resources()const{return allocated||lease.active||inTransaction;}
 void poll(){++polls;}
 void configure(const uint32_t*,uint32_t wanted,bool(*)(void*,uint32_t)){++configures;allocated=true;lease.active=wanted&allowedTags;state=lease.active==wanted?Ready:Slots;}
 void retire(){++retires;if(!inTransaction){lease.active=0;lease.uncertain=false;allocated=false;}}
 void audit(){++audits;}
 void inspect(){++inspections;}
};
}namespace singlefps {
static bool desired=false,enabled=false;static int failure=0;static uint32_t signatureRevision=0;
static techbp::SignatureSnapshot<6> native;
static void disable(){enabled=false;}static void maintain(){}
}}
namespace subject {
struct Context{uint32_t Eip=assist::controlparams::Entry,Esp=Stack,Eax=99,Ebx=98,Ecx=Target,Edx=97,Ebp=96,Esi=95,Edi=94,EFlags=0x202;};
struct Shared{uint32_t controlAll=controlmode::Parameters,stop=0,flags=4096,mapVisibility=0;}storage,*sh=&storage;
static constexpr unsigned UnsafeTech=4096;
static unsigned techImplementation=techmode::Parameters;static bool fault=false;static uint32_t lastHouse=House;
static DWORD sampleClock(Shared*){return GetTickCount();}
namespace controlparams {
using assist::controlparams::AdmissionImage;using assist::controlparams::Entry;using assist::controlparams::destination;using assist::controlparams::world;
static bool guardedRead(uint32_t address,void*out,size_t size){return localRead(address,out,size);}
}
extern "C" int GuardedControlWorldEligible();
namespace techparameters {
static constexpr uint32_t FullGroup=1;
#include "control-types.inc"
static Policy policy;static techbp::Service service;
static bool controlEligible=false,desired=false,enhancementDesired=false,controlDesired=false,mapDesired=false,enhancementCompareRequested=false,enhancementCameoBlocked=false,mapRedrawPending=false;
static int failure=0,enhancementFailure=0,controlFailure=0,mapFailure=0;
static DWORD auditAt=0,retryAt=0;static uint32_t attempted=0,refusedGroups=0,resourceGroups=0;
static uint32_t worldCaptures=0,controlAdmissionChecks=0,nativePolicyRevision=0,enhancementPolicyRevision=0,controlPolicyRevision=0,mapPolicyRevision=0;
static volatile LONG controlExceptions=0;
static FailureRetry fullFailureRetry,controlFailureRetry;
static techbp::SignatureSnapshot<20>nativeDraw;static techbp::SignatureSnapshot<15>nativePrune;
static techbp::SignatureSnapshot<16>nativeEntry,enhancementCompare,enhancementEntry;
static techbp::SignatureSnapshot<13>enhancementLimit;static techbp::SignatureSnapshot<10>mapSignature;
static constexpr uint32_t sites[techbp::SiteCount]={0x4F7870,mapvisibility::Address,0x4F7951,0x4F7B66,controlparams::Entry,singlefps::Entry};
static unsigned publishes=0;static void publish(){++publishes;}
static bool callback(void*,uint32_t){return false;}
#include "control-helpers.inc"
}
#include "control-cold.inc"
extern "C" int GuardedControlWorldEligible(){++coldCalls;return modeledNoReport([](){return AssistControlWorldEligible();});}
#include "control-lifecycle.inc"
static int dispatch(Context&context,unsigned tag){using namespace techparameters;
#include "control-hot.inc"
 return 0;
}
}
using namespace subject;
using namespace subject::techparameters;
static const Trace WorldReads={{0xA83D4C,4},{House+0x10,4},{0xA8B230,4},{0xA8B238,4},{0xA8ED84,4},{0xA8E9A0,1},{0xAC1198,4},{House+0x1F5,1},{House+0x1F6,1},{House+0x1F8,1}};
static void seed(){
 memory.clear();trace.clear();readObserver=nullptr;throwAt=throwCode=throwFlags=0;lastError=0x1234;clockValue+=1000;
 storage={};sh=&storage;fault=false;lastHouse=House;techImplementation=techmode::Parameters;
 policy=Policy{};service=techbp::Service{};desired=enhancementDesired=controlDesired=mapDesired=controlEligible=enhancementCompareRequested=enhancementCameoBlocked=mapRedrawPending=false;
 failure=enhancementFailure=controlFailure=mapFailure=0;auditAt=retryAt=attempted=refusedGroups=resourceGroups=0;fullFailureRetry.reset();controlFailureRetry.reset();
 nativePolicyRevision=enhancementPolicyRevision=controlPolicyRevision=mapPolicyRevision=0;controlExceptions=0;
 singlefps::desired=false;singlefps::disable();singlefps::failure=0;singlefps::signatureRevision=0;
 debugentries::activePolicy={};++debugentries::policyRevision;syncDebugBreakpointPolicy();
 put<uint32_t>(0xA83D4C,House);put<uint32_t>(House+0x10,10);put<uint32_t>(0xA8B230,Scenario);put<int>(0xA8B238,3);put<int>(0xA8ED84,100);put<uint8_t>(0xA8E9A0,1);put<uint32_t>(0xAC1198,0);
 put<uint8_t>(House+0x1F5,0);put<uint8_t>(House+0x1F6,0);put<uint8_t>(House+0x1F8,0);put<uint32_t>(Stack,0x650005);
 for(auto range:{std::pair<uint32_t,unsigned>{0x50B6F0,0x34},{0x6339B0,0x1C},{0x637E37,0x1B}})for(unsigned n=0;n<range.second;++n)put<uint8_t>(range.first+n,0);
 for(const auto&p:patchcontext::Probes)for(unsigned n=0;n<p.size;++n)put<uint8_t>(p.address+n,p.bytes[n]);
 for(const auto&s:mastercontrol::Sites)for(unsigned n=0;n<5;++n)put<uint8_t>(s.address+n,s.before[n]);
 const uint8_t prefix[]={0x85,0xC0,0x75,0x1D},tail[]={0xB8,1,0,0,0,0xC3,0x8B,0x15,0x4C,0x3D,0xA8,0,0x33,0xC0,0x3B,0xCA,0x0F,0x94,0xC0,0xC3};
 for(unsigned n=0;n<sizeof prefix;++n)put<uint8_t>(0x50B6F5+n,prefix[n]);for(unsigned n=0;n<sizeof tail;++n)put<uint8_t>(0x50B710+n,tail[n]);
}
static void activate(){
 const auto unchanged=memory;trace.clear();check(requestControlParameters()==int(controlmode::Parameters),"actual request admits native control signatures");
 check(controlEligible&&policy.controlling&&controlDesired&&!controlFailure&&service.activeTags()==16,"ready and eligible publish control");
 check(trace==WorldReads,"cold publication reads original ten fields in order");check(memory==unchanged&&lastError==0x1234,"cold admission does not mutate game and preserves error");
}
static void hot(bool allowed,uint32_t caller=0x650005,uint32_t target=Target,uint32_t esp=Stack,unsigned reads=1){
 put<uint32_t>(Stack,caller);Context context;context.Ecx=target;context.Esp=esp;auto expected=context;
 const auto unchanged=memory;trace.clear();const LONG oldCount=controlExceptions;const unsigned oldCold=coldCalls;
 check(modeledNoReport([&](){return dispatch(context,4);})==int(allowed),"actual hot callback result");
 if(allowed)expected.Eip=assist::controlparams::destination(caller);
 check(!std::memcmp(&context,&expected,sizeof context),"callback changes only native leaf EIP on acceptance");
 check(memory==unchanged&&trace.size()==reads&&coldCalls==oldCold,"hot read budget and no cold authorization work");
 if(reads)check(trace[0]==std::make_pair(esp,size_t(4)),"sole hot game read is caller stack slot");
 check(controlExceptions==oldCount+1,"owner hit counted once");++hotCalls;
}
static void maintain(){trace.clear();maintainTechParameters();++transitions;}
static void worldMutation(unsigned kind){
 switch(kind){case 0:put<uint32_t>(0xA83D4C,Target);break;case 1:put<uint32_t>(House+0x10,11);break;case 2:put<uint32_t>(0xA8B230,Scenario+0x1000);break;case 3:put<int>(0xA8B238,4);break;case 4:put<int>(0xA8ED84,99);break;case 5:put<uint8_t>(0xA8E9A0,0);break;case 6:put<uint32_t>(0xAC1198,House);break;case 7:put<uint8_t>(House+0x1F5,1);break;case 8:put<uint8_t>(House+0x1F6,1);break;case 9:put<uint8_t>(House+0x1F8,1);break;}
}
static void boundarySemantics(){
 // captureWorld alone does not test running/observer/defeat flags. A first
 // request must run the complete cold guard before publishing permission.
 for(unsigned kind=5;kind<10;++kind){seed();const auto original=memory;worldMutation(kind);trace.clear();
  check(requestControlParameters()==3&&controlDesired&&service.activeTags()==16,"initial invalid world may retain only requested DR readiness");
  check(!controlEligible&&!policy.controlling&&!controlFailure,"initial request does not mistake captureWorld for full eligibility");hot(false,0x650005,Target,Stack,0);
  memory=original;maintain();check(controlEligible&&policy.controlling,"initial temporarily ineligible world recovers at maintenance");hot(true);
 }
 for(unsigned kind=0;kind<10;++kind){seed();activate();const auto original=memory;worldMutation(kind);
  hot(true);maintain();check(!controlEligible&&!policy.controlling&&controlDesired&&!controlFailure,"world mutation revokes at maintenance without failure latch");
  check(trace.size()==kind+1&&service.activeTags()==16,"cold guard short-circuits but retains DR lease");hot(false,0x650005,Target,Stack,0);
  desired=true;mapDesired=true;configure();check(!policy.controlling,"Full/map sibling configure cannot revive revoked world permission");
  memory=original;maintain();check(controlEligible&&policy.controlling,"valid world automatically resumes next maintenance");hot(true);
 }
 // An unchanged logical frame never acts as a refresh/expiry condition.
 seed();activate();for(unsigned i=0;i<12;++i)hot(true);put<uint8_t>(0xA8E9A0,0);hot(true);maintain();hot(false,0x650005,Target,Stack,0);
 maintain();check(!policy.controlling&&service.activeTags()==16,"pause keeps dormant lease across repeated maintenance");put<uint8_t>(0xA8E9A0,1);maintain();hot(true);
 // Every byte of the old world guard can become unreadable independently.
 for(auto field:WorldReads)for(size_t byte=0;byte<field.second;++byte){seed();activate();const auto original=memory;memory.erase(field.first+uint32_t(byte));hot(true);maintain();
  check(!controlEligible&&!policy.controlling&&!controlFailure&&service.activeTags()==16,"local unreadable world suppresses only permission");
  memory=original;maintain();hot(true);
 }
 // ECX is not dereferenced by either native leaf. Target identity/roster data
 // are deliberately irrelevant before AND after a world maintenance pass.
 for(uint32_t caller:{0x650005u,assist::controlparams::OwnerReturn,assist::controlparams::PlanningReturn})for(uint32_t target:{0u,0xffffu,Target,0x7fffffffu,0xffffffffu}){
  seed();activate();put<uint32_t>(0xA8022C,0);put<int>(0xA80238,33);put<int>(Target+0x30,-1);put<uint32_t>(Target+0x10,0);hot(true,caller,target);maintain();hot(true,caller,target);
 }
}
static void immediateGates(){
 for(unsigned kind=0;kind<9;++kind){seed();activate();
  switch(kind){case 0:storage.stop=1;break;case 1:fault=true;break;case 2:sh=nullptr;break;case 3:storage.controlAll=controlmode::Off;break;case 4:storage.controlAll=controlmode::Native;break;case 5:storage.controlAll=controlmode::Master;break;case 6:storage.controlAll=99;break;case 7:policy.controlling=0;break;case 8:++controlPolicyRevision;break;}
  hot(false,0x650005,Target,Stack,0);
  if(kind<7){maintain();check(!controlEligible&&!policy.controlling&&!controlDesired,"stop/fault/mode loss revokes stored eligibility and request");}
 }
 seed();activate();hot(false,0,Target);hot(false,0xffff,Target);hot(false,0x650005,Target,0xffff,0);hot(false,0x650005,Target,0x7ffffffc,1);
 throwAt=Stack;throwCode=0xc0000005;hot(false);check(lastError==0x1234,"bad caller read preserves LastError");
 seed();activate();restoreControlParameters();check(!controlEligible&&!policy.controlling&&!controlDesired&&service.activeTags()==0,"explicit restore revokes and retires");desired=true;configure();check(!policy.controlling,"sibling after restore cannot revive");
 seed();activate();storage.controlAll=controlmode::Off;hot(false,0x650005,Target,Stack,0);storage.controlAll=controlmode::Parameters;hot(true);
 storage.controlAll=controlmode::Off;maintain();storage.controlAll=controlmode::Parameters;hot(false,0x650005,Target,Stack,0);
 check(requestControlParameters()==3&&policy.controlling,"mode reenable after observed retirement requires fresh request");hot(true);
}
static void generationAndPolicy(){
 for(unsigned kind=0;kind<5;++kind){seed();activate();
  switch(kind){case 0:put<uint32_t>(0xA8B230,Scenario+0x1000);break;case 1:put<int>(0xA8B238,4);break;case 2:put<int>(0xA8ED84,99);break;case 3:put<uint32_t>(House+0x10,11);break;case 4:lastHouse=Target;put<uint32_t>(0xA83D4C,Target);put<uint32_t>(Target+0x10,20);put<uint8_t>(Target+0x1F5,0);put<uint8_t>(Target+0x1F6,0);put<uint8_t>(Target+0x1F8,0);break;}
  check(captureWorld(lastHouse),"actual shared capture admits new generation");check(!controlEligible&&!policy.controlling,"changed captured generation immediately clears eligibility");
  desired=true;configure();check(!policy.controlling,"capturing sibling cannot authorize changed generation");maintain();check(controlEligible&&policy.controlling,"fresh ten-field maintenance may publish new captured generation");
 }
 seed();activate();auto cfg=debugentries::activePolicy;check(debugentries::setSkip(cfg,debugentries::id(debugentries::Kind::Breakpoint,assist::controlparams::Entry),true)&&debugentries::applyPolicy(cfg),"change actual control signature policy");hot(false,0x650005,Target,Stack,0);
 syncDebugBreakpointPolicy();check(!controlEligible&&!policy.controlling&&!controlDesired&&!controlPolicyRevision,"changed control policy revokes publication and request");desired=true;configure();check(!policy.controlling,"policy-revoked eligibility cannot revive via sibling");check(requestControlParameters()==3&&policy.controlling,"explicit fresh admission publishes after policy change");
 seed();activate();cfg=debugentries::activePolicy;check(debugentries::setSkip(cfg,debugentries::id(debugentries::Kind::Breakpoint,mapvisibility::Address),true)&&debugentries::applyPolicy(cfg),"change sibling policy");hot(false,0x650005,Target,Stack,0);maintain();check(controlEligible&&policy.controlling&&controlPolicyRevision==debugentries::policyRevision,"unrelated policy revision refreshes control identity");
}
static void failuresAndPending(){
 seed();activate();service.inTransaction=true;maintain();check(controlEligible&&!policy.controlling&&service.activeTags()==16&&!controlFailure,"pending transaction conservatively withholds publication");hot(false,0x650005,Target,Stack,0);
 service.inTransaction=false;maintain();check(policy.controlling,"normal maintenance after transaction restores valid permission");
 service.inTransaction=true;put<uint8_t>(0xA8E9A0,0);maintain();check(!controlEligible&&!policy.controlling&&service.activeTags()==16&&!controlFailure,"invalid world during pending never revives");service.inTransaction=false;maintain();check(!policy.controlling,"pending completion alone cannot revive invalid world");put<uint8_t>(0xA8E9A0,1);maintain();hot(true);
 for(int state:{techbp::System,techbp::Foreign,techbp::Thread}){seed();activate();service.state=state;maintain();check(!controlEligible&&!policy.controlling&&controlFailure==state,"unhealthy service revokes eligibility");
  service.state=techbp::Ready;clockValue+=501;check(requestControlParameters()==3&&policy.controlling,"retired healthy service can retry admission");}
 seed();service.allowedTags=0;check(requestControlParameters()<0&&controlEligible&&!policy.controlling,"eligible world alone cannot publish missing slot");service.allowedTags=techbp::AllTags;clockValue+=501;maintain();check(policy.controlling,"ready slot plus refreshed eligibility publishes");
 seed();activate();desired=true;service.allowedTags=16;configure();check(service.state==techbp::Slots&&policy.controlling,"confirmed existing control slot survives sibling exhaustion");
 seed();activate();const auto original=memory;memory[assist::controlparams::Entry]^=1;check(requestControlParameters()<0&&!controlEligible&&!policy.controlling&&!controlDesired,"signature failure revokes before any publication");
 memory=original;clockValue+=501;check(requestControlParameters()==3&&policy.controlling,"signature retry after cooldown revalidates");
 seed();activate();controlFailure=techbp::Signature;controlFailureRetry.defer();check(requestControlParameters()<0&&!controlEligible&&!policy.controlling,"retry deferral cannot retain old true");
 for(unsigned kind=0;kind<6;++kind){seed();activate();switch(kind){case 0:put<uint32_t>(0xA83D4C,Target);break;case 1:put<uint32_t>(0xA8B230,0);break;case 2:put<int>(0xA8ED84,-1);break;case 3:put<int>(0xA8B238,2);break;case 4:put<uint32_t>(House+0x10,0);break;case 5:memory.erase(0xA8B230);break;}
  check(requestControlParameters()==0&&!controlEligible&&!policy.controlling&&!controlDesired,"capture failure clears old authorization");desired=true;configure();check(!policy.controlling,"failed capture cannot be resurrected by sibling");}
}
static void observeRevoked(){
 readObserver=nullptr;check(!controlEligible&&!policy.controlling,"publication false before first potentially faulting game read");configure();check(!policy.controlling,"reentrant sibling configure sees false-first");hot(false,0x650005,Target,Stack,0);
}
static void exceptionTransitions(){
 for(uint32_t code:{0xc0000005u,0xc0000006u,0x80000003u,0x80000004u,0xe06d7363u,0xe0424242u}){
  seed();activate();throwAt=0xA83D4C;throwCode=code;bool escaped=false;try{maintain();}catch(const MemoryFault&caught){check(caught.code==code,"unrelated exception identity preserved");escaped=true;}
  check(escaped==!assistguard::localAccessFailure(code,0),"only AV/in-page handled by local production policy");
  check(!controlEligible&&!policy.controlling&&lastError==0x1234&&!controlFailure,"all exits preserve error and leave old permission revoked");
  configure();check(!policy.controlling,"sibling cannot resurrect permission after handled or propagated fault");throwAt=0;maintain();hot(true);
 }
 seed();activate();throwAt=0xA83D4C;throwCode=0xc0000005;throwFlags=2;bool escaped=false;try{maintain();}catch(const MemoryFault&){escaped=true;}
 check(escaped&&!controlEligible&&!policy.controlling,"unwind-tagged AV is not swallowed and remains revoked");
 seed();activate();readObserver=observeRevoked;maintain();check(controlEligible&&policy.controlling,"successful refresh publishes only after guarded reader returns");
}
int main(){
 boundarySemantics();immediateGates();generationAndPolicy();failuresAndPending();exceptionTransitions();
 std::printf("{\"pass\":true,\"checks\":%u,\"hot_callback_cases\":%u,\"cold_guard_calls\":%u,\"maintenance_calls\":%u,\"successful_hot_game_reads\":1,\"full_cold_world_reads\":10,\"semantics_changed\":true,\"target_house_validation_removed\":true,\"world_refresh_boundary\":\"ordinary owner maintenance or fresh request\",\"hard_refresh_latency_bound_claimed\":false,\"service_and_exception_models\":true,\"real_windows_seh_executed\":false,\"game_process_executed\":false}\n",checks,hotCalls,coldCalls,transitions);
}
