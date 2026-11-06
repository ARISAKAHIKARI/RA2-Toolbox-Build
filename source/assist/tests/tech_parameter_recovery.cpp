#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using DWORD=uint32_t;using LONG=int32_t;
static DWORD clockNow=1000;
static DWORD GetTickCount(){return clockNow;}
static LONG InterlockedExchange(volatile LONG*p,LONG value){LONG old=*p;*p=value;return old;}
static int checks=0,fullProbes=0,controlProbes=0,worldProbes=0,captureProbes=0,legacyProbes=0;
static bool fullCompatible=true,controlCompatible=true,worldCurrent=true,legacyRetired=true;
static void check(bool condition,const char*message){++checks;if(!condition){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
struct Shared {uint32_t flags=1,controlAll=3;bool stop=false,mapVisibility=false;};
static Shared shared;static Shared*sh=&shared;static bool fault=false;
static uint32_t techImplementation=4,lastHouse=0x10000;
static constexpr uint32_t UnsafeTech=1;
static DWORD sampleClock(Shared*){return clockNow;}
template<class T>static T get(uint32_t){return T(1);}
static bool read(uint32_t,void*,size_t){return true;}
namespace techmode {static bool parameters(uint32_t mode){return mode==4;}}
namespace controlmode {static constexpr int Parameters=3;static bool parameters(uint32_t mode){return mode==3;}}
namespace controlparams {template<class Read>static bool compatible(Read){++controlProbes;return controlCompatible;}}
namespace mapvisibility {static constexpr uint32_t Tag=2;}
namespace singlefps {
static constexpr uint32_t Tag=32;static bool desired=false,enabled=false;static int failure=0;
static void disable(){enabled=false;}static void maintain(){desired=false;failure=0;}
}
namespace techbp {
static constexpr unsigned AllTags=63,SiteCount=6;
enum Status {Off=0,Ready=1,Waiting=2,Signature=-1,Slots=-2,System=-3,Foreign=-4,Thread=-5};
struct Lease {
 uint32_t mask=0,uncertain=0,groups=0;
 uint32_t tags()const{return groups;}
};
// Explicit boundary model: only the worker's verified outcome is modeled.
// Retirement holds, ownership uncertainty and pending jobs can be injected;
// production groupOff/configure decides whether admission is permitted.
struct Service {
 Lease lease;int state=Off,lastError=0;bool retained=false,pendingJob=false,holdRetirement=false;
 uint32_t pendingGroups=0;int configurations=0,retirements=0,audits=0,inspections=0;
 void poll(){}
 bool pending()const{return pendingJob;}
 bool pendingFor(uint32_t group)const{return pendingJob&&(pendingGroups&group);}
 bool resources()const{return retained||lease.mask||pendingJob;}
 uint32_t activeTags()const{return lease.uncertain?0:lease.tags();}
 void own(uint32_t tags){lease.groups=tags;lease.mask=tags?1:0;lease.uncertain=0;retained=true;state=tags?Ready:Off;}
 bool configure(const uint32_t*,uint32_t wanted,bool(*)(void*,uint32_t)){++configurations;own(wanted);return true;}
 bool audit(){++audits;return true;}
 bool inspect(){++inspections;return true;}
 bool retire(){
  ++retirements;if(pendingJob||holdRetirement)return false;
  if(lease.mask){lease={};state=Off;return false;}
  retained=false;state=Off;return true;
 }
};
}
namespace techparameters {
static constexpr uint32_t FullGroup=1;
struct Policy {volatile LONG enabled=0,enhanced=0,controlling=0,showing=0;};
static Policy policy;static techbp::Service service;
static bool mapDesired=false;static int mapFailure=0;
static constexpr uint32_t sites[techbp::SiteCount]={1,2,3,4,5,6};
static uint32_t enhancementTags(){return 4;}
static bool callback(void*,uint32_t){return false;}
static int publishes=0;static void publish(){++publishes;}
static bool nativeCompatible(){++fullProbes;return fullCompatible;}
static bool captureWorld(uint32_t house){++worldProbes;return worldCurrent&&house==lastHouse;}
static bool capture(uint32_t house){++captureProbes;return captureWorld(house);}
}
static bool restoreTechEnhancement(){++legacyProbes;return legacyRetired;}
#include "tech_parameter_recovery_subject.inc"

static bool request(bool full){return full?requestTechParameters(lastHouse):requestControlParameters()==controlmode::Parameters;}
static int&failureFor(bool full){return full?techparameters::failure:techparameters::controlFailure;}
static bool&compatibleFor(bool full){return full?fullCompatible:controlCompatible;}
static int probesFor(bool full){return full?fullProbes:controlProbes;}
static uint32_t tagFor(bool full){return full?1:16;}
static void mode(bool full){sh->flags=full?UnsafeTech:0;sh->controlAll=full?0:controlmode::Parameters;}
static void advance(DWORD delta){clockNow+=delta;}
static void failSignature(bool full){
 compatibleFor(full)=false;check(!request(full),"first bad signature is rejected");
 check(failureFor(full)==techbp::Signature,"signature diagnostic remains local and visible");
}
static void recoverSignature(bool full){
 failSignature(full);const int original=probesFor(full);compatibleFor(full)=true;
 advance(499);check(!request(full),"signature failure does not retry before 500 ms");
 check(probesFor(full)==original,"cooldown avoids another compatibility probe");
 advance(1);check(request(full),full?"Full recovers after a transient signature failure":"control recovers after a transient signature failure");
 check(failureFor(full)==0,"successful revalidation clears only its failure");
 check(probesFor(full)==original+1,"recovery uses a fresh compatibility probe");
}
static void serviceFailure(bool full,int error){
 check(request(full),"feature first acquires its group");
 techparameters::service.state=error;maintainTechParameters();
 check(failureFor(full)==error,"shared worker failure is retained for the feature");
 maintainTechParameters();check(!techparameters::service.resources(),"failed old service completes retirement");
 const int original=probesFor(full);advance(499);check(!request(full),"service failure observes the cooldown");
 check(probesFor(full)==original,"failed service cannot cause a premature signature probe");
 advance(1);check(request(full),full?"Full recovers after its service failure retires":"control recovers after its service failure retires");
 check(failureFor(full)==0,"retired service failure is cleared after validation");
}
int main(int argc,char**argv){
 if(argc!=2)return 2;const char*name=argv[1];const bool full=std::strncmp(name,"control_",8)!=0;mode(full);
 if(std::strstr(name,"transient_signature"))recoverSignature(full);
 else if(!std::strcmp(name,"full_service_failure")||!std::strcmp(name,"control_service_failure"))serviceFailure(full,techbp::System);
 else if(!std::strcmp(name,"full_foreign_failure"))serviceFailure(true,techbp::Foreign);
 else if(!std::strcmp(name,"control_thread_failure"))serviceFailure(false,techbp::Thread);
 else if(!std::strcmp(name,"both_service_failure")){
  sh->flags=UnsafeTech;sh->controlAll=controlmode::Parameters;
  check(request(true)&&request(false),"Full and control can be active together");
  techparameters::service.state=techbp::System;maintainTechParameters();maintainTechParameters();
  check(techparameters::failure==techbp::System&&techparameters::controlFailure==techbp::System,"shared failure is visible to both requested groups");
  advance(500);check(request(true)&&request(false),"both groups recover without a user mode toggle");
  check(techparameters::service.activeTags()==17,"both verified groups are restored");
 }
 else if(std::strstr(name,"persistent_signature")){
  failSignature(full);const int original=probesFor(full);
  for(int n=0;n<3;++n){advance(n?1:500);check(!request(full),"truly incompatible signature remains rejected");
   check(failureFor(full)==techbp::Signature,"persistent rejection keeps the original local error");
   check(probesFor(full)==original+n+1,"persistent failure retries only at its cold deadline");
   advance(499);check(!request(full),"persistent failure is still bounded between deadlines");
   check(probesFor(full)==original+n+1,"persistent failure does not probe on every request");}
  check(!techparameters::service.activeTags(),"invalid signature never acquires a hardware group");
 }
 else if(std::strstr(name,"healthy_sibling")){
  sh->flags=UnsafeTech;sh->controlAll=controlmode::Parameters;
  check(request(!full),"healthy sibling starts first");const uint32_t sibling=tagFor(!full);
  failSignature(full);check(techparameters::service.activeTags()==sibling,"failed signature preserves the healthy sibling");
  compatibleFor(full)=true;advance(500);check(request(full),"recovery is allowed while a healthy sibling owns shared resources");
  check(techparameters::service.activeTags()==17,"recovery retains the sibling while adding the recovered group");
  check(!failureFor(!full),"a local signature failure never marks its sibling failed");
 }
 else if(std::strstr(name,"old_lease")||std::strstr(name,"uncertain_lease")||std::strstr(name,"pending_transaction")){
  failSignature(full);compatibleFor(full)=true;auto&s=techparameters::service;const bool uncertain=std::strstr(name,"uncertain_lease"),pending=std::strstr(name,"pending_transaction");
  s.own(uncertain?tagFor(!full):tagFor(full));s.holdRetirement=true;
  if(uncertain)s.lease.uncertain=1;
  if(pending){s.pendingJob=true;s.pendingGroups=tagFor(full);}
  const int original=probesFor(full);advance(500);
  check(!request(full),"a live or uncertain old lease blocks revalidation");
  check(probesFor(full)==original,"blocked retirement performs no new signature probe");
  check(failureFor(full)==techbp::Signature,"blocked retirement retains the failure diagnostic");
  s.pendingJob=false;s.holdRetirement=false;s.lease={};s.retained=false;s.state=techbp::Off;
  check(request(full),"revalidation resumes after actual group retirement");
 }
 else if(std::strstr(name,"failed_retirement")){
  check(request(full),"feature is active before worker failure");auto&s=techparameters::service;s.state=techbp::System;s.holdRetirement=true;
  maintainTechParameters();const int original=probesFor(full);advance(1000);maintainTechParameters();
  check(!request(full),"unhealthy shared service cannot be reacquired");
  check(probesFor(full)==original,"failed retirement never probes or acquires again");
  check(failureFor(full)==techbp::System,"failed retirement retains its system diagnostic");
  s.holdRetirement=false;maintainTechParameters();maintainTechParameters();advance(500);
  check(request(full),"worker retirement can recover without clearing live ownership early");
 }
 else if(std::strstr(name,"clock_wrap")){clockNow=UINT32_MAX-200;recoverSignature(full);}
 else if(std::strstr(name,"clock_zero")){clockNow=0;recoverSignature(full);}
 else if(std::strstr(name,"disable_reset")){
  failSignature(full);advance(10);if(full)sh->flags=0;else sh->controlAll=0;maintainTechParameters();
  check(!failureFor(full),"disable clears the previous local failure");
  if(full)sh->flags=UnsafeTech;else sh->controlAll=controlmode::Parameters;
  compatibleFor(full)=true;check(request(full),"explicit reenable does not inherit an obsolete retry deadline");
 }
 else if(!std::strcmp(name,"stop_no_reactivation")){
  sh->flags=UnsafeTech;sh->controlAll=controlmode::Parameters;failSignature(true);failSignature(false);sh->stop=true;advance(500);
  const int before=fullProbes+controlProbes;check(!request(true)&&!request(false),"stop cannot reactivate either failed group");
  check(fullProbes+controlProbes==before,"stop does not start compatibility probes");
  check(!techparameters::failure&&!techparameters::controlFailure,"stop clears local retry state");
  check(!techparameters::service.activeTags(),"stop leaves no active group");
 }
 else if(std::strstr(name,"world_recheck")||!std::strcmp(name,"full_restore_recheck")){
  failSignature(full);compatibleFor(full)=true;const bool legacy=!std::strcmp(name,"full_restore_recheck");
  if(legacy)legacyRetired=false;else worldCurrent=false;advance(500);
  check(!request(full),"fresh admission requires current world and legacy restoration");
  check(failureFor(full)==techbp::Signature,"incomplete admission preserves the old failure diagnostic");
  if(!full)check(requestControlParameters()==techbp::Signature-10,"control returns the existing local error until world revalidation succeeds");
  const int probed=probesFor(full);advance(499);check(!request(full),"incomplete world admission is also bounded");
  check(probesFor(full)==probed,"world retry does not repeat signatures every callback");
  legacyRetired=worldCurrent=true;advance(1);check(request(full),"recovered world permits a freshly validated admission");
 }
 else return 2;
 std::printf("{\"case\":\"%s\",\"checks\":%d}\n",name,checks);return 0;
}
