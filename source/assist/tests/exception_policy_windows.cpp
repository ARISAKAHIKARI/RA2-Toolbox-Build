#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <initializer_list>
#ifdef NDEBUG
#error Exception policy verification requires assertions.
#endif
extern "C" unsigned FixtureOuter(unsigned);
extern "C" unsigned FixtureScopedOuter(void*);
extern "C" {unsigned fixtureCode=0;int fixtureResult=-1;}
static volatile unsigned*noAccess=nullptr;
static unsigned captureDepth=0,begins=0,ends=0,reports=0,lastBoundary=0,lastCode=0,globalDisables=0,localFailures=0;
static bool actualAccess=false,faultEnabled=true;
static unsigned routeCalls[7]={};
static void fault(){if(!faultEnabled)return;if(actualAccess)*noAccess=5;else RaiseException(fixtureCode,0,0,nullptr);}
extern "C" unsigned BeginAssistCapture(){++begins;SetLastError(0x1111);return ++captureDepth;}
extern "C" void EndAssistCapture(unsigned token){assert(token==captureDepth&&captureDepth);--captureDepth;++ends;SetLastError(0x2222);}
extern "C" void AssistReportException(unsigned boundary,unsigned code,unsigned,unsigned,unsigned){++reports;lastBoundary=boundary;lastCode=code;}
extern "C" void AssistFault(){++globalDisables;}
extern "C" void AssistTick(){++routeCalls[0];fault();}
extern "C" int AssistFilterMessage(void*p){assert(p==(void*)7);++routeCalls[1];fault();return 8;}
extern "C" int AssistRiskAction(unsigned a,unsigned b,unsigned c){assert(a==3&&b==4&&c==5&&captureDepth==1);++routeCalls[2];fault();return 9;}
extern "C" int AssistSidebarViewAction(unsigned a,unsigned b){assert(a==6&&b==7&&captureDepth==1);++routeCalls[3];fault();return 10;}
extern "C" void AssistSidebarViewFailed(){++localFailures;}
extern "C" int AssistTechUiParameter(void*p,unsigned tag){assert(p==(void*)8&&tag==9);++routeCalls[4];fault();return 11;}
extern "C" void CaptureCursorResource(){++routeCalls[5];fault();}
extern "C" void CursorResourceFault(){++localFailures;}
extern "C" int AssistControlWorldEligible(){++routeCalls[6];fault();return 1;}
// Unused entrypoint callbacks required to link the complete product guard.
// None is invoked by these routing tests; there is no engine/game fixture.
extern "C" int AssistNetworkSend(unsigned,unsigned,int,int,int){assert(false);return 0;}
extern "C" void AssistNetworkSenderAbort(){assert(false);}
extern "C" unsigned AssistBuildTimeTooltip(unsigned,unsigned original){assert(false);return original;}
extern "C" void AssistBuildTimeTooltipFailed(){assert(false);}
extern "C" unsigned AssistMapCrcShadow(unsigned,unsigned,unsigned value){assert(false);return value;}
extern "C" void AssistMapCrcAbort(){assert(false);}
extern "C" unsigned AssistPauseViewMainGate(){assert(false);return 0;}
extern "C" void AssistPauseViewFailed(){assert(false);}
extern "C" unsigned AssistPauseViewNative(unsigned,unsigned,unsigned,unsigned){assert(false);return 0;}
extern "C" unsigned BeginNativeViewQuery(){assert(false);return 0;}
extern "C" void EndNativeViewQuery(unsigned){assert(false);}
extern "C" int AssistNativeViewAction(unsigned,unsigned){assert(false);return 0;}
extern "C" void AssistNativeViewFailed(){assert(false);}
extern "C" void AssistNativeViewUnlockOne(){assert(false);}
extern "C" unsigned AssistNativeViewWndEnter(unsigned,unsigned){assert(false);return 0;}
extern "C" unsigned AssistNativeViewWndLeave(){assert(false);return 0;}
extern "C" int AssistNativeViewMousePoint(void*){assert(false);return 0;}
extern "C" int __stdcall AssistTechEnhancementProbeCall(unsigned,unsigned){assert(false);return 0;}
extern "C" unsigned BeginTechEnhancementProbe(){assert(false);return 0;}
extern "C" void EndTechEnhancementProbe(unsigned){assert(false);}
struct ScopeState {unsigned mode,phase,cleanups,resumes;};
static ScopeState*scopeState=nullptr;
extern "C" void FixtureScopeBody(void*raw){
 auto&state=*static_cast<ScopeState*>(raw);assert(&state==scopeState&&state.phase==0&&state.cleanups==0);state.phase=1;
 if(state.mode==1)*noAccess=7;
 if(state.mode==2){RaiseException(EXCEPTION_BREAKPOINT,0,0,nullptr);assert(state.cleanups==0&&state.phase==1&&state.resumes==1);}
 state.phase=2;
}
extern "C" void FixtureScopeCleanup(void*raw){
 auto&state=*static_cast<ScopeState*>(raw);assert(&state==scopeState&&state.cleanups==0&&state.phase==(state.mode==1?1u:2u));
 ++state.cleanups;state.phase=3;
}
static LONG WINAPI resumeScopeDebug(EXCEPTION_POINTERS*pointers){
 if(scopeState&&scopeState->mode==2&&pointers->ExceptionRecord->ExceptionCode==EXCEPTION_BREAKPOINT){
  assert(scopeState->phase==1&&scopeState->cleanups==0);++scopeState->resumes;return EXCEPTION_CONTINUE_EXECUTION;
 }
 return EXCEPTION_CONTINUE_SEARCH;
}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 noAccess=static_cast<volatile unsigned*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));assert(noAccess);
 unsigned checks=0,reentries=0;
 // Exercise a second call after an outer handler consumes the forwarded
 // exception. This proves guard-owned capture cleanup, not Runtime's caller
 // scheduler/game-entry ownership (which this fixture does not model).
 const auto nextCall=[&](unsigned route){
  const auto beforeCalls=routeCalls[route],beforeReports=reports,beforeFailures=localFailures;
  faultEnabled=false;SetLastError(0x6789);assert(FixtureOuter(route)==0);faultEnabled=true;
  assert(routeCalls[route]==beforeCalls+1&&fixtureResult==(route==0?0:route==1?8:route==2?9:route==3?10:route==4?11:route==6?1:12));
  assert(reports==beforeReports&&localFailures==beforeFailures&&globalDisables==0&&captureDepth==0&&begins==ends);
  if(route==2||route==3||route==4||route==6)assert(GetLastError()==0x6789);++reentries;
 };
 for(unsigned route=0;route<7;++route){
  for(unsigned code:{0x80000003u,0x80000004u,0x40010005u,0x40010008u,0x80000026u,0xe06d7363u,0xe0424242u}){
   fixtureCode=code;actualAccess=false;const auto beforeReports=reports,beforeFailures=localFailures;
   SetLastError(0x789a);assert(FixtureOuter(route)==1);assert(reports==beforeReports&&localFailures==beforeFailures&&globalDisables==0&&captureDepth==0&&begins==ends);
   if(route==2||route==3||route==4||route==6)assert(GetLastError()==0x789a);++checks;nextCall(route);
  }
 }
 // The exact global boundaries report a genuine native memory fault, then
 // let this test's outer SEH handler observe it. They never disable Hooks.
 for(unsigned route=0;route<3;++route){
  fixtureCode=EXCEPTION_ACCESS_VIOLATION;actualAccess=true;const auto beforeReports=reports,beforeFailures=localFailures;
  SetLastError(0x89ab);assert(FixtureOuter(route)==1);
  assert(reports==beforeReports+1&&lastCode==EXCEPTION_ACCESS_VIOLATION&&lastBoundary==(route==0?1:route==1?10:11));
  assert(localFailures==beforeFailures&&globalDisables==0&&captureDepth==0&&begins==ends);if(route==2)assert(GetLastError()==0x89ab);++checks;nextCall(route);
 }
 // The existing sidebar access-failure contract remains local, including
 // its finally cleanup and LastError; no global failure callback runs.
 fixtureCode=EXCEPTION_ACCESS_VIOLATION;actualAccess=true;const auto beforeReports=reports,beforeFailures=localFailures;
 SetLastError(0x9abc);assert(FixtureOuter(3)==0);
 assert(reports==beforeReports+1&&lastBoundary==19&&localFailures==beforeFailures+1&&globalDisables==0&&captureDepth==0&&begins==ends&&GetLastError()==0x9abc);++checks;nextCall(3);
 // A no-report tech classification AV stays a local rejection. Debug and
 // language/control exceptions for this exact guard were forwarded above.
 fixtureCode=EXCEPTION_ACCESS_VIOLATION;actualAccess=true;
 {const auto beforeReports=reports,beforeFailures=localFailures;
  assert(FixtureOuter(4)==0&&fixtureResult==0&&reports==beforeReports&&localFailures==beforeFailures&&globalDisables==0);++checks;nextCall(4);}
 // Cold control eligibility uses the same production no-report boundary.
 // Actual AV and a raised in-page code both return false, preserve LastError,
 // and do not latch a feature/global fault. Other codes forwarded above.
 for(unsigned code:{0xc0000005u,0xc0000006u}){
  fixtureCode=code;actualAccess=code==0xc0000005u;
  const auto beforeReports=reports,beforeFailures=localFailures;
  SetLastError(0xabcd);assert(FixtureOuter(6)==0&&fixtureResult==0);
  assert(reports==beforeReports&&localFailures==beforeFailures&&globalDisables==0&&GetLastError()==0xabcd);++checks;nextCall(6);
 }
 // Cursor resource capture has a defined local access-failure fallback.
 // It must not intercept debug/control exceptions (tested above).
 fixtureCode=EXCEPTION_ACCESS_VIOLATION;actualAccess=true;
 {const auto beforeReports=reports,beforeFailures=localFailures;
  assert(FixtureOuter(5)==0&&fixtureResult==12&&reports==beforeReports&&localFailures==beforeFailures+1&&globalDisables==0);++checks;nextCall(5);}
 // The same production finally trampoline must keep ownership throughout
 // resumed debug dispatch, and clean once on normal return or outer unwind.
 void*resumeHandler=AddVectoredExceptionHandler(1,resumeScopeDebug);assert(resumeHandler);
 for(unsigned mode=0;mode<3;++mode){
  ScopeState state{mode,0,0,0};scopeState=&state;fixtureCode=EXCEPTION_ACCESS_VIOLATION;
  assert(FixtureScopedOuter(&state)==(mode==1?1u:0u));
  assert(state.phase==3&&state.cleanups==1&&state.resumes==(mode==2?1u:0u));scopeState=nullptr;
 }
 assert(RemoveVectoredExceptionHandler(resumeHandler));
 assert(VirtualFree(const_cast<unsigned*>(noAccess),0,MEM_RELEASE));
 std::printf("PASS real Windows x86 guard.cpp + cursor resource_guard.cpp: %u routing cases, %u normal reentries; debug/control and global AV reach outer SEH; local AV fallback preserves capture; zero global disables; finally normal/unwind/resumed-debug passed\n",checks,reentries);
 return 0;
}
