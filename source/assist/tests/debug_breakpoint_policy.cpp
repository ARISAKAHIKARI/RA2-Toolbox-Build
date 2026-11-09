// Production cold admission, hot snapshots, policy transitions and status
// sampling. Windows memory/timer/DR boundaries are explicit synthetic models.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include "tech_ui_breakpoint_debug.h"
#include "technology_mode_policy.h"
#include "control_mode_policy.h"
#include "control_parameter_policy.h"
#include "map_visibility_parameter_policy.h"
#include "tech_enhancement_parameter_policy.h"
#include "single_player_fps.h"
#include "read_only_lifecycle.h"
using namespace assist;
using DWORD=uint32_t;using LONG=int32_t;using HANDLE=void*;using LONGLONG=int64_t;
struct LARGE_INTEGER {int64_t QuadPart=0;};
struct CONTEXT {uint32_t Eip=0,Esi=0,EFlags=0;};
static constexpr uint32_t UnsafeTech=4096,ExperimentalReveal=0x04000000;
static constexpr int FALSE=0,TIMER_ALL_ACCESS=1,WAIT_OBJECT_0=0,ERROR_NOT_SUPPORTED=50;
static uint32_t now=1000,unreadable=0;static uint64_t qpc=1000;static int64_t timerDelay=0;
static bool callable=true,ares=false,mutateDuringValidation=false,executable=true;
static unsigned checks=0,nativeChecks=0,timersCreated=0;
static unsigned readCalls=0,readBytes=0,controlReads=0,controlBytes=0,baselineControlReads=0,baselineControlBytes=0;
static std::vector<std::pair<uint32_t,uint32_t>> ranges;
static void check(bool ok,const char*what){++checks;if(!ok){std::fprintf(stderr,"FAIL %u %s\n",checks,what);std::exit(1);}}
static DWORD GetTickCount(){return now;}
static LONG InterlockedExchange(volatile LONG*p,LONG value){const LONG old=*p;*p=value;return old;}
static void MemoryBarrier(){}
static bool read(uint32_t at,void*out,size_t n){
 ++readCalls;readBytes+=unsigned(n);
 if(unreadable>=at&&uint64_t(unreadable)<uint64_t(at)+n)return false;
 for(auto range:ranges)if(at>=range.first&&uint64_t(at)+n<=uint64_t(range.first)+range.second){std::memcpy(out,reinterpret_cast<void*>(uintptr_t(at)),n);return true;}
 return false;
}
template<class T>static T get(uint32_t at){T value{};read(at,&value,sizeof value);return value;}
template<class T>static void put(uint32_t at,T value){std::memcpy(reinterpret_cast<void*>(uintptr_t(at)),&value,sizeof value);}
static bool nativeEntryCallable(uint32_t at,bool=false){check(at==0x4F7870,"only the strict CanBuild boundary is queried");++nativeChecks;if(mutateDuringValidation)*reinterpret_cast<uint8_t*>(uintptr_t(at+15))^=1;return callable;}
static bool nativeAresAvailable(){return ares;}
static bool nativeExecutableRange(uint32_t at,size_t n){uint8_t bytes[16]{};return executable&&n<=sizeof bytes&&read(at,bytes,n);}
static bool QueryPerformanceFrequency(LARGE_INTEGER*out){out->QuadPart=10000000;return true;}
static bool QueryPerformanceCounter(LARGE_INTEGER*out){out->QuadPart=int64_t(qpc);return true;}
static HANDLE CreateWaitableTimerExW(void*,void*,unsigned,unsigned){++timersCreated;return reinterpret_cast<HANDLE>(uintptr_t(1));}
static HANDLE CreateWaitableTimerW(void*,int,void*){++timersCreated;return reinterpret_cast<HANDLE>(uintptr_t(1));}
static bool SetWaitableTimer(HANDLE,const LARGE_INTEGER*delay,int,void*,void*,int){timerDelay=delay->QuadPart;return true;}
static DWORD WaitForSingleObject(HANDLE,DWORD){qpc+=uint64_t(-timerDelay);return WAIT_OBJECT_0;}
static bool CloseHandle(HANDLE){return true;}
struct Shared {
 uint32_t flags=UnsafeTech,stop=0,heartbeat=1000,debugVisible=0;
 uint32_t techEnhance=0,techEnhanceImplementation=techenhanceparams::Exact,techEnhanceCompareLevel=0;
 uint32_t controlAll=0,mapVisibility=0,renderReveal=0;
 uint32_t singlePlayerFPS=0,singlePlayerFPSActive=0,singlePlayerFPSHits=0;
 uint32_t readOnly=0,readOnlyRequest=0,readOnlyAck=0;
} wire;
static Shared*sh=&wire;static bool fault=false;static uint32_t techImplementation=4;
static readonly::RuntimeState readOnlyState;
static DWORD sampleClock(Shared*){return now;}
#include "debug_breakpoint_lease.inc"
namespace assist { namespace techbp {
// The kernel transaction is modeled. Lease/tag classification and activeTags
// itself are the unchanged production implementations extracted above/below.
struct Service {
 Lease lease;LONG state=Off,lastError=0,confirmed=0;bool retained=false,pendingJob=false;uint32_t pendingGroups=0;
 void poll(){}
 bool pendingFor(uint32_t group){return pendingJob&&(pendingGroups&group);}
 bool resources()const{return retained||lease.mask||pendingJob;}
#include "debug_breakpoint_active.inc"
};
}}
#include "debug_breakpoint_fps.inc"
namespace techparameters {
static constexpr uint32_t FullGroup=1;
struct Policy {volatile LONG enabled=0,enhanced=0,controlling=0,showing=0,compareLevel=0;}policy;
static techbp::Service service;
static bool world(){return true;}
}
#include "debug_breakpoint_subject.inc"

static const uint8_t Original[]={0x83,0xEC,0x3C,0x8A,0x44,0x24,0x44,0x53,0x55,0x8B,0xE9,0x56,0x57,0x84,0xC0,0x89};
static const uint8_t AresEntry[]={0xE9,0x8B,0x87,0xDE,0x04,0x90,0x90,0x53,0x55,0x8B,0xE9,0x56,0x57,0x84,0xC0,0x89};
static const uint8_t Draw[]={0x8B,0x0D,0x4C,0x3D,0xA8,0x00,0x6A,0x00,0x6A,0x00,0x50,0x89,0x44,0x24,0x38,0xE8,0x99,0xE0,0xE4,0xFF};
static const uint8_t Prune[]={0x8B,0x88,0x1C,0x02,0x00,0x00,0x6A,0x01,0x57,0x53,0xE8,0xEA,0xD0,0xE4,0xFF};
static const uint8_t Fps[]={0x8b,0x35,0x60,0xeb,0xa8,0,0x8d,0x4c,0x24,0x14,0xe8,0x93,0xb4,0x16,0,0x8b,0x4c,0x24,0x14,0xa3,0x48,0x73,0x88,0,0x89,0x0d,0x4c,0x73,0x88,0,0x89,0x35,0x50,0x73,0x88,0};
template<size_t N>static void bytes(uint32_t at,const uint8_t(&data)[N]){std::memcpy(reinterpret_cast<void*>(uintptr_t(at)),data,N);}
static void flip(uint32_t at){*reinterpret_cast<uint8_t*>(uintptr_t(at))^=1;}
static void skip(debugentries::Kind kind,uint32_t at,bool on){auto config=debugentries::activePolicy;check(debugentries::setSkip(config,debugentries::id(kind,at),on),"explicit policy toggle changes the selected stable identity");check(debugentries::applyPolicy(config),"runtime consumes the changed policy");}
static const debugentries::Observation&observation(uint32_t at){const auto*p=debugentries::observation(debugentries::Kind::Breakpoint,at);check(p!=nullptr,"DR address has a real catalogue row");return *p;}
static void seedControl(){
 for(const auto&s:mastercontrol::Sites){bytes(s.address,s.before);for(const auto&p:patchcontext::Probes)if(p.site==s.address)std::memcpy(reinterpret_cast<void*>(uintptr_t(p.address)),p.bytes,p.size);}
 const uint8_t prefix[]={0x85,0xC0,0x75,0x1D};bytes(0x50B6F5,prefix);
 const uint8_t tail[]={0xB8,1,0,0,0,0xC3,0x8B,0x15,0x4C,0x3D,0xA8,0,0x33,0xC0,0x3B,0xCA,0x0F,0x94,0xC0,0xC3};bytes(0x50B710,tail);
}
static void reset(){
 using namespace techparameters;
 for(auto r:ranges)std::memset(reinterpret_cast<void*>(uintptr_t(r.first)),0,r.second);
 now+=1000;wire={};wire.heartbeat=now;sh=&wire;fault=false;callable=true;ares=false;mutateDuringValidation=false;executable=true;unreadable=0;
 auto config=debugentries::Config{};config.generation=debugentries::activePolicy.generation+1;if(!config.generation)config.generation=1;debugentries::applyPolicy(config);debugentries::clockTick=now;
 policy={};service={};desired=enhancementDesired=controlDesired=mapDesired=false;failure=enhancementFailure=controlFailure=mapFailure=0;refusedGroups=resourceGroups=attempted=0;fullFailureRetry.reset();controlFailureRetry.reset();
 nativePolicyRevision=enhancementPolicyRevision=controlPolicyRevision=mapPolicyRevision=0;debugExactUnreachable=false;nativeStructureRejected=false;syncDebugBreakpointPolicy();
 singlefps::release();bytes(0x4F7870,Original);bytes(0x6A97C3,Draw);bytes(0x6AA777,Prune);bytes(mapvisibility::PrefixAddress,mapvisibility::Prefix);bytes(0x55D79E,Fps);seedControl();
 put<int>(0xA8B238,5);put<uint32_t>(0xA8B230,0x18000000);put<int>(0xA8ED84,100);put<uint8_t>(0xA8E9A0,1);put<uint32_t>(singlefps::TimerDuration,7);
}
static void fullChecks(){
 using namespace techparameters;using debugentries::Kind;
 reset();check(nativeCompatible()&&nativeCompatibleHot(),"strict original Full admission and hot image pass");
 for(auto range:{std::pair<uint32_t,unsigned>{0x6A97C3,20},{0x6AA777,15},{0x4F7870,16}})for(unsigned i=0;i<range.second;++i){flip(range.first+i);check(!nativeCompatibleHot(),"every admitted byte remains checked on the hot path");flip(range.first+i);check(nativeCompatibleHot(),"restored bytes match the captured hot image");}
 flip(0x6A97C3);check(!nativeCompatible(),"strict Full rejects a changed caller");
 skip(Kind::Call,0x4F7870,true);check(!nativeCompatible(),"Call override cannot bypass DR checks at the same address");
 skip(Kind::Breakpoint,0x4F7870,true);check(nativeCompatible()&&nativeCompatibleHot(),"Full DR override captures the actual readable image");
 check(observation(0x4F7870).signature==debugentries::Signature::Skipped,"reference bypass is reported as skipped, never matched");
 flip(0x6A97C3+1);check(!nativeCompatibleHot(),"a post-admission change is not bypassed on a hot hit");flip(0x6A97C3+1);
 skip(Kind::Breakpoint,0x4F7870,false);check(!nativeCompatibleHot(),"turning off invalidates the old policy image immediately");syncDebugBreakpointPolicy();check(!nativeCompatible(),"turning off restores strict cold admission");
 skip(Kind::Breakpoint,0x4F7870,true);unreadable=0x6AA777;check(!nativeCompatible(),"an override never accepts an unreadable caller");unreadable=0;
 mutateDuringValidation=true;check(!nativeCompatible(),"changed image during uncached native validation is rejected");mutateDuringValidation=false;executable=false;check(!nativeCompatible(),"nonexecutable entry remains structurally rejected");
 wire.flags=UnsafeTech;failure=techbp::Signature;publishDebugBreakpointStates();check(observation(0x4F7870).reason==debugentries::Reason::Structural,"a real structural rejection is not mislabeled as reference mismatch");
 reset();bytes(0x4F7870,AresEntry);ares=true;check(nativeCompatible(),"verified Ares Full call semantics remain admitted");check(observation(0x4F7870).signature==debugentries::Signature::Platform,"known platform admission is distinguished");
}
static void exactChecks(){
 using namespace techparameters;using debugentries::Kind;
 reset();wire.techEnhance=1;check(!enhancementCompatible(),"unknown exact comparison bytes are strict by default");
 skip(Kind::Breakpoint,0x4F7951,true);check(enhancementCompatible(),"unused optional comparison does not block the first exact DR entry");
 wire.techEnhanceCompareLevel=1;check(!enhancementCompatible(),"the second exact entry needs its independent override");
 skip(Kind::Breakpoint,0x4F7B66,true);check(enhancementCompatible(),"two explicit exact overrides capture both readable comparison contexts");policy.compareLevel=1;
 check(enhancementCompatibleHot(),"exact hot path uses the accepted current images");flip(0x4F7B5C);check(!enhancementCompatibleHot(),"exact comparison mutation is still rejected hot");flip(0x4F7B5C);
 unreadable=0x4F7870;check(!enhancementCompatible(),"the shared exact entry must remain readable even with both overrides");unreadable=0;
 skip(Kind::Breakpoint,0x4F7951,false);check(!enhancementCompatible(),"restoring one exact entry to strict does not inherit the sibling bypass");
 bytes(0x4F7870,AresEntry);ares=true;check(enhancementBodyBypassed(),"known Ares path bypass remains independently detectable");enhancementFailure=techenhanceparams::NativeBodyBypassed;publishDebugBreakpointStates();
 check(observation(0x4F7951).reason==debugentries::Reason::PathUnreachable,"unreachable exact path is never labeled installed by a request");
}
static void mapAndControlChecks(){
 using namespace techparameters;using debugentries::Kind;
 reset();wire.mapVisibility=1;check(mapCompatible(),"strict map reference passes");policy.showing=1;CONTEXT context{mapvisibility::Address,0,0x246};check(mapVisibilityParameter(context)&&!(context.EFlags&0x40),"actual map callback uses the admitted snapshot");
 flip(mapvisibility::PrefixAddress);check(!mapCompatible(),"map reference mutation rejects by default");skip(Kind::Breakpoint,mapvisibility::Address,true);check(mapCompatible(),"map override captures the actual readable prefix");context.EFlags=0x246;check(mapVisibilityParameter(context),"map hot hit matches its debug-admitted image");flip(mapvisibility::PrefixAddress+1);context.EFlags=0x246;check(!mapVisibilityParameter(context)&&context.EFlags==0x246,"map hot mutation cannot alter flags");
 unreadable=mapvisibility::PrefixAddress;check(!mapCompatible(),"map override preserves read failure");
 reset();check(controlCompatible(),"strict control branch/caller references pass");flip(0x6339BA);check(!controlCompatible(),"changed control caller is rejected");skip(Kind::Patch,controlparams::Entry,true);check(!controlCompatible(),"Patch override does not bypass the Control DR entry");
 skip(Kind::Breakpoint,controlparams::Entry,true);check(controlCompatible(),"Control DR override is independently effective");unreadable=0x50B710;check(!controlCompatible(),"later unreadable strict leaf rejects even after an earlier mismatched caller");unreadable=0;
 skip(Kind::Breakpoint,controlparams::Entry,false);check(!controlCompatible(),"Control DR returns to strict after override closes");
 reset();flip(0x50B6F9);check(!controlCompatible(),"changed control context is rejected without an override");skip(Kind::Patch,controlparams::Entry,true);check(!controlCompatible(),"Patch context override cannot leak into strict Control DR admission");skip(Kind::Breakpoint,controlparams::Entry,true);check(controlCompatible(),"only the independent Control DR choice may bypass its context reference");
}
static void controlAdmissionReads(){
 using namespace techparameters;using debugentries::Kind;
 auto verify=[&](bool expected){
  readCalls=readBytes=0;const bool accepted=controlCompatible();
  check(accepted==expected,"batched control admission preserves the reference/readability result");
  check(readCalls==3&&readBytes==107,"control admission reads each required range exactly once");
#ifdef CONTROL_ADMISSION_BASELINE
  readCalls=readBytes=0;const bool previous=controlCompatibleBaseline();
  check(previous==accepted,"fixed previous production admission agrees with current result");
#endif
 };
 reset();readCalls=readBytes=0;check(controlCompatible(),"control performance baseline starts with valid references");
 controlReads=readCalls;controlBytes=readBytes;check(controlReads==3&&controlBytes==107,"successful admission performs three bounded reads");
#ifdef CONTROL_ADMISSION_BASELINE
 readCalls=readBytes=0;check(controlCompatibleBaseline(),"previous production references remain valid");
 baselineControlReads=readCalls;baselineControlBytes=readBytes;check(baselineControlReads==24,"fixed previous admission repeats twenty-four reads");
#endif
 const uint8_t entries[][9]={{0xA1,0x38,0xB2,0xA8,0,0x85,0xC0,0x75,0x1D},{0xE9,0xFB,0x2A,0x67,0,0xCC,0xCC,0xCC,0xCC},{0xE9,0x8B,0x2C,0x67,0,0xCC,0xCC,0xCC,0xCC}};
 for(const auto&entry:entries){
  reset();bytes(controlparams::Entry,entry);verify(true);
  for(auto range:{std::pair<uint32_t,unsigned>{0x50B6F0,52},{0x6339B0,28},{0x637E37,27}})for(unsigned i=0;i<range.second;++i){
   flip(range.first+i);verify(false);flip(range.first+i);
  }
  skip(Kind::Breakpoint,controlparams::Entry,true);flip(0x6339BA);
  verify(true);
  for(auto range:{std::pair<uint32_t,unsigned>{0x50B6F0,52},{0x6339B0,28},{0x637E37,27}})for(unsigned i=0;i<range.second;++i){
   unreadable=range.first+i;verify(false);
  }
  unreadable=0;verify(true);flip(0x6339BA);
  skip(Kind::Breakpoint,controlparams::Entry,false);verify(true);
 }
 reset();controlparams::AdmissionImage image;check(image.capture(read),"fixed control image captured");
 uint8_t output[53];std::memset(output,0xA5,sizeof output);
 check(!image.copy(0x50B6F0,output,sizeof output)&&output[0]==0xA5,"snapshot refuses a read extending beyond a verified range");
 check(!image.copy(0x50B6EF,output,1)&&output[0]==0xA5,"snapshot refuses a read before its fixed range");
 check(!image.copy(0xFFFFFFFFu,output,2)&&output[0]==0xA5,"snapshot refuses wrapping addresses");
 unreadable=0x637E45;check(!image.capture(read)&&!image.copy(controlparams::Entry,output,5),"a failed recapture cannot reuse earlier readable bytes");
}
static void fpsChecks(){
 using debugentries::Kind;
 reset();wire.singlePlayerFPS=45;singlefps::maintain();check(singlefps::desired&&!singlefps::failure,"actual FPS maintenance admits the strict prefix");singlefps::enabled=true;CONTEXT context{singlefps::Entry,7,0x246};check(singlefps::parameter(context)&&context.Eip==singlefps::Resume&&context.Esi==7&&context.EFlags==0x246,"actual FPS callback preserves registers and changes only its defined destination");
 flip(0x55D79E);singlefps::maintain();check(!singlefps::desired&&singlefps::failure==techbp::Signature,"FPS mismatch rejects before timer permission");skip(Kind::Breakpoint,singlefps::Entry,true);singlefps::maintain();check(singlefps::desired,"FPS override admits current readable bytes");singlefps::enabled=true;context={singlefps::Entry,7,0x246};check(singlefps::parameter(context),"FPS hot path uses the override snapshot");
 flip(0x55D79E + 1);context={singlefps::Entry,7,0x246};check(!singlefps::parameter(context),"FPS hot byte changes still reject");flip(0x55D79E + 1);
 unreadable=0x55D79E;singlefps::maintain();check(!singlefps::desired,"FPS override does not bypass unreadability");unreadable=0;
 skip(Kind::Breakpoint,singlefps::Entry,false);singlefps::maintain();check(!singlefps::desired,"FPS returns to strict immediately");
}
static void statusAndTransitionChecks(){
 using namespace techparameters;using namespace debugentries;
 reset();check(nativeCompatible(),"Full baseline established");desired=true;policy.enabled=1;
 techbp::Registers registers;check(service.lease.configure(registers,sites,FullGroup)==techbp::Ready,"production lease chooses a physical slot");service.state=techbp::Ready;service.confirmed=0;
 publishDebugBreakpointStates();check(observation(0x4F7870).hook!=Hook::Installed,"UI request and unconfirmed lease never imply installed");
 service.confirmed=1;publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Installed,"confirmed lease is reported installed");
 service.pendingJob=true;service.pendingGroups=1;publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Pending,"a pending kernel transaction is reported pending");service.pendingJob=false;
 service.lease.uncertain=1;publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Pending,"ambiguous ownership is never installed");service.lease.uncertain=0;
 service.state=techbp::Foreign;publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Conflict&&observation(0x4F7870).reason==Reason::Foreign,"foreign DR state remains a conflict");
 service.state=techbp::Slots;wire.controlAll=controlmode::Parameters;refusedGroups=16;publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Installed,"slot shortage retains the confirmed healthy sibling");check(observation(controlparams::Entry).hook==Hook::Unavailable&&observation(controlparams::Entry).error==techbp::Slots,"missing requested group is reported as slot shortage");
 service.state=techbp::Ready;const auto owned=service.lease;const auto nativeBefore=nativeEntry;const uint32_t revisionBefore=nativePolicyRevision;wire.debugVisible=1;
 inspectDebugBreakpointSignatures();check(!std::memcmp(&owned,&service.lease,sizeof owned)&&!std::memcmp(&nativeBefore,&nativeEntry,sizeof nativeBefore)&&nativePolicyRevision==revisionBefore,"visible diagnostics do not alter installed lease or hot admission");
 const unsigned created=timersCreated;inspectDebugBreakpointSignatures();check(timersCreated==created,"read-only debug probes never create an FPS timer");
 skip(Kind::Breakpoint,mapvisibility::Address,true);syncDebugBreakpointPolicy();check(desired&&policy.enabled&&service.activeTags()==1&&nativeCompatibleHot(),"unrelated DR override preserves a healthy Full group");
 skip(Kind::Breakpoint,0x4F7870,true);syncDebugBreakpointPolicy();check(!desired&&!policy.enabled&&service.lease.mask==owned.mask,"changed Full policy stops its permission without discarding its lease");
 check(service.lease.restore(registers)==techbp::Off&&!registers.control,"production cleanup restores the actual saved register ownership");service.confirmed=0;service.state=techbp::Off;service.retained=false;wire.flags=0;wire.controlAll=0;publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Inactive,"retired unrequested entry is inactive");
 reset();check(nativeCompatible(),"valid native signature before unsupported DR capability");service.state=techbp::System;service.lastError=ERROR_NOT_SUPPORTED;failure=techbp::System;
 publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Unavailable&&observation(0x4F7870).reason==Reason::Unsupported&&observation(0x4F7870).error==ERROR_NOT_SUPPORTED,"unconfirmed hardware capability is not mislabeled as machine-code structure mismatch");
 failure=techbp::Signature;publishDebugBreakpointStates();check(observation(0x4F7870).reason==Reason::Signature,"the independent signature refusal remains higher priority than service capability failure");
 failure=techbp::System;service.pendingJob=true;service.pendingGroups=1;publishDebugBreakpointStates();check(observation(0x4F7870).hook==Hook::Pending&&observation(0x4F7870).reason==Reason::OwnerPending,"pending owned cleanup keeps precedence over unsupported status");
}
int main(){
 for(auto range:{std::pair<uint32_t,uint32_t>{0x4F7000,0x2000},{0x6A9000,0x2000},{0x6D8000,0x2000},{0x50B000,0x2000},{0x633000,0x1000},{0x637000,0x2000},{0x55D000,0x1000},{0xA8B000,0x4000},{0x887000,0x1000}}){void*p=mmap(reinterpret_cast<void*>(uintptr_t(range.first)),range.second,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);check(p==reinterpret_cast<void*>(uintptr_t(range.first)),"private fixture region reserved");ranges.push_back(range);}
 fullChecks();exactChecks();mapAndControlChecks();controlAdmissionReads();fpsChecks();statusAndTransitionChecks();
 std::printf("{\"passed\":true,\"checks\":%u,\"control_admission_reads\":%u,\"control_admission_bytes\":%u,\"baseline_control_admission_reads\":%u,\"baseline_control_admission_bytes\":%u,\"production_functions_extracted\":true,\"kernel_and_memory_boundaries_modeled\":true,\"windows_executed\":false,\"game_tested\":false}\n",checks,controlReads,controlBytes,baselineControlReads,baselineControlBytes);
}
