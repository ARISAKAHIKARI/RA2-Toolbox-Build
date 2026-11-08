// The complete production DR Service and the actual bridge configure/maintain
// functions run together. Win32 and INT3 delivery reuse the documented boundary
// from breakpoint_owner_rendezvous.cpp; this is not Windows or game execution.
#include "idle-inspection-boundary.inc"

using namespace assist;
namespace assist { namespace singlefps {
static constexpr uint32_t Tag=32;
static bool desired=false,enabled=false;static int failure=0;
static void disable(){enabled=false;}
static void maintain(){} // FPS admission is outside this idle-diagnostics test.
}}
namespace subject {
struct Shared {uint32_t flags=0,controlAll=0,mapVisibility=0,debugVisible=0,hardwareSlotBudget=0;bool stop=false;}wire,*sh=&wire;
static constexpr uint32_t UnsafeTech=1;
static uint32_t techImplementation=0,lastHouse=0x10000;
static bool fault=false,running=true,eligible=true;
static unsigned gameReads=0,eligibilityReads=0,publications=0;
static DWORD sampleClock(Shared*){return GetTickCount();}
template<class T>static T get(uint32_t address){CHECK(address==0xA8E9A0);++gameReads;return T(running);}
namespace techmode {static bool parameters(uint32_t mode){return mode==4;}}
namespace controlmode {static bool parameters(uint32_t mode){return mode==3;}}
namespace mapvisibility {static constexpr uint32_t Tag=2;}
namespace debugentries {static uint32_t policyRevision=1;}
static int GuardedControlWorldEligible(){++eligibilityReads;return eligible;}
namespace techparameters {
static constexpr uint32_t FullGroup=1;
struct Policy {volatile LONG enabled=0,enhanced=0,controlling=0,showing=0;}policy;
static Service service;
static bool mapDesired=false;static int mapFailure=0;
static uint32_t controlPolicyRevision=1;
static constexpr uint32_t sites[SiteCount]={0x401000,0x402000,0x403000,0x404000,0x405000,0x406000};
#include "idle-inspection-globals.inc"
static uint32_t enhancementTags(){return enhancementCompareRequested?12u:4u;}
static void syncDebugBreakpointPolicy(){} // Its unchanged policy is tested separately.
static void publish(){++publications;if(sh)sh->hardwareSlotBudget=service.slotBudget();}
#include "idle-inspection-configure.inc"
}
#include "idle-inspection-maintain.inc"
}
using namespace subject;
using namespace subject::techparameters;

static void setRequests(uint32_t groups){
 desired=(groups&1)!=0;mapDesired=(groups&2)!=0;enhancementDesired=(groups&4)!=0;
 enhancementCompareRequested=(groups&8)!=0;controlDesired=(groups&16)!=0;singlefps::desired=(groups&32)!=0;
 sh->flags=desired?UnsafeTech:0;techImplementation=desired?4:0;
 sh->controlAll=controlDesired?3:0;sh->mapVisibility=mapDesired;
}
static void maintain(){maintainTechParameters();}
static void retireAll(){
 setRequests(0);sh->debugVisible=0;fault=false;sh->stop=false;
 for(unsigned i=0;i<8&&service.resources();++i)maintain();
 CHECK(!service.resources()&&!handler&&!Service::instance());
}
static bool sameDebug(const CONTEXT&a,const CONTEXT&b){return Service::same(Service::registers(a),Service::registers(b))&&a.Dr6==b.Dr6;}
static bool sameLeaseRegisters(const CONTEXT&a,const CONTEXT&b){return Service::same(Service::registers(a),Service::registers(b));}
static void seedForeign(){current->context.Dr0=0x77770000;current->context.Dr7=1;current->context.Dr6=0x8000;}

int main(int argc,char**argv){
 if(argc!=2)return 2;const char*name=argv[1];scenario(name);active=&service;
 const unsigned beforeTraps=trapCalls,beforeDup=duplicateCalls,beforeClose=closeCalls;
 const unsigned beforeAdds=registrationCalls,beforeRemoves=removalCalls,beforeWrites=writes;
 if(!std::strcmp(name,"idle_hidden")||!std::strcmp(name,"idle_visible")){
  const bool visible=!std::strcmp(name,"idle_visible");sh->debugVisible=visible;
  seedForeign();const CONTEXT original=current->context;
  for(unsigned i=0;i<10000;++i){ticks=100+i;maintain();CHECK(!service.occupied()&&!singlefps::enabled);}
  CHECK(sameDebug(original,current->context));CHECK(writes==beforeWrites&&!witnessCalls);
  if(visible){CHECK(trapCalls-beforeTraps==20);CHECK(slotbudget::known(sh->hardwareSlotBudget));CHECK(slotbudget::foreign(sh->hardwareSlotBudget)==1);}
  // Baseline is measured with the same assertions. Only the runner expects
  // hidden inspection work to disappear in the repaired production source.
  retireAll();sh->stop=true;ticks+=1001;maintain();CHECK(!slotbudget::known(sh->hardwareSlotBudget));
 }
 else if(!std::strcmp(name,"visibility_transition")){
  sh->debugVisible=1;maintain();CHECK(trapCalls==beforeTraps+1&&slotbudget::known(sh->hardwareSlotBudget));
  sh->debugVisible=0;ticks+=500;maintain();CHECK(!service.resources());
  const unsigned hiddenTraps=trapCalls;for(unsigned i=0;i<1000;++i){++ticks;maintain();}
  CHECK(trapCalls==hiddenTraps);CHECK(!slotbudget::known(sh->hardwareSlotBudget));
  sh->debugVisible=1;maintain();CHECK(trapCalls==hiddenTraps+1&&slotbudget::known(sh->hardwareSlotBudget));retireAll();
 }
 else if(!std::strcmp(name,"requested_hidden")||!std::strcmp(name,"active_audit_hidden")){
  seedForeign();const CONTEXT original=current->context;setRequests(16);maintain();
  CHECK(service.activeTags()==16&&policy.controlling&&witnessCalls==1&&witnessHits==1);
  CHECK(slotbudget::known(sh->hardwareSlotBudget)&&slotbudget::foreign(sh->hardwareSlotBudget)==1);
  const unsigned activated=trapCalls;hit(sites[4]);CHECK(callbacks==1);
  if(!std::strcmp(name,"active_audit_hidden")){
   for(unsigned i=1;i<=1000;++i){ticks=100+i;maintain();CHECK(service.activeTags()==16&&policy.controlling);}
   CHECK(trapCalls==activated+2); // first audit, then its 500 ms cadence
  }else CHECK(trapCalls==activated); // the feature hit is not a private INT3
  retireAll();CHECK(sameLeaseRegisters(original,current->context));
  const unsigned retired=trapCalls;ticks+=501;maintain();CHECK(trapCalls==retired&&!service.resources());
 }
 else if(!std::strcmp(name,"disabled_cleanup_hidden")||!std::strcmp(name,"stop_cleanup_hidden")||!std::strcmp(name,"fault_cleanup_hidden")){
  seedForeign();const CONTEXT original=current->context;setRequests(16);maintain();CHECK(service.activeTags()==16);
  if(!std::strcmp(name,"disabled_cleanup_hidden"))sh->controlAll=0;
  else if(!std::strcmp(name,"stop_cleanup_hidden"))sh->stop=true;else fault=true;
  maintain();CHECK(!service.occupied()&&!policy.controlling&&sameLeaseRegisters(original,current->context));
  maintain();CHECK(!service.resources());retireAll();
 }
 else if(!std::strcmp(name,"foreign_cleanup_hidden")){
  setRequests(16);maintain();CHECK(service.activeTags()==16);current->context.Dr0=0x76543210;
  ticks+=501;maintain();CHECK(service.state==Foreign&&!service.occupied());
  maintain();CHECK(current->context.Dr0==0x76543210&&(current->context.Dr7&1));retireAll();
 }
 else if(!std::strcmp(name,"pending_cleanup_hidden")){
  setRequests(16);maintain();CHECK(service.activeTags()==16);const CONTEXT installed=current->context;
  service.job_=1;service.operationGroups_=16;sh->controlAll=0;const unsigned before=trapCalls;
  maintain();CHECK(service.occupied()&&service.resources()&&trapCalls==before&&sameDebug(installed,current->context));
  service.job_=0;maintain();CHECK(!service.occupied());maintain();CHECK(!service.resources());retireAll();
 }
 else if(!std::strcmp(name,"failed_cleanup_hidden")){
  setRequests(16);maintain();CHECK(service.activeTags()==16);sh->controlAll=0;ignoreWrites=true;
  maintain();CHECK(service.occupied()&&service.resources()&&service.state==System&&!policy.controlling);
  ignoreWrites=false;maintain();maintain();CHECK(!service.resources());retireAll();
 }
 else if(!std::strcmp(name,"fault_visible")||!std::strcmp(name,"stop_visible")||!std::strcmp(name,"no_house_visible")||!std::strcmp(name,"not_running_visible")){
  sh->debugVisible=1;
  if(!std::strcmp(name,"fault_visible"))fault=true;
  else if(!std::strcmp(name,"stop_visible"))sh->stop=true;
  else if(!std::strcmp(name,"no_house_visible"))lastHouse=0;else running=false;
  for(unsigned i=0;i<1000;++i){ticks=100+i;maintain();}
  CHECK(!service.resources()&&trapCalls==beforeTraps&&duplicateCalls==beforeDup);retireAll();
 }
 else if(!std::strcmp(name,"clock_wrap_visible")){
  sh->debugVisible=1;ticks=0xffffff00u;maintain();CHECK(trapCalls==beforeTraps+1);
  ticks+=499;maintain();CHECK(trapCalls==beforeTraps+1);
  ++ticks;maintain();CHECK(trapCalls==beforeTraps+2);retireAll();
 }
 else return 2;
 CHECK(!service.resources()&&!handler&&!Service::instance());
 CHECK(!waitInfinite&&!productionSuspends&&!productionResumes&&!productionWorkers);
 for(auto*h:handles){CHECK(h->closed);delete h;}
 std::printf("{\"case\":\"%s\",\"passed\":true,\"checks\":%u,\"virtual_window_ms\":%u,\"private_rendezvous\":%u,\"handler_registrations\":%u,\"handler_removals\":%u,\"thread_handles_duplicated\":%u,\"thread_handles_closed\":%u,\"debug_register_writes\":%u,\"game_running_reads\":%u,\"eligibility_reads\":%u,\"windows_executed\":false,\"game_executed\":false}\n",name,checks,(!std::strcmp(name,"idle_hidden")||!std::strcmp(name,"idle_visible"))?10000u:0u,trapCalls-beforeTraps,registrationCalls-beforeAdds,removalCalls-beforeRemoves,duplicateCalls-beforeDup,closeCalls-beforeClose,writes-beforeWrites,gameReads,eligibilityReads);
}
