#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include "../../shared/source/game_entry_client.h"
#include "../../shared/source/runtime_fp_scope.h"

// The execution manager and the shared scope helpers are the production
// implementations. Only guard callbacks unrelated to this fixture are stubbed
// by exception_policy_windows.cpp, whose main is not used here.
extern "C" unsigned FixtureRuntimeScopeOuter(void*);
struct Case {
 unsigned mode=0,entered=0,completed=0,cleanups=0,captureCleanups=0,resumed=0;
 volatile LONG dispatch=0;
 void* noAccess=nullptr;
};
static Case* currentCase=nullptr;
static unsigned short controlWord(){unsigned short value;__asm {fnstcw value}return value;}
static unsigned mxcsr(){unsigned value;__asm {stmxcsr value}return value;}
static void controlWord(unsigned short value){__asm {fldcw value}}
static void mxcsr(unsigned value){__asm {ldmxcsr value}}
static LONG CALLBACK resumeDebug(EXCEPTION_POINTERS* pointers){
 if(currentCase&&currentCase->mode==2&&pointers->ExceptionRecord->ExceptionCode==EXCEPTION_BREAKPOINT){
  assert(currentCase->cleanups==0&&currentCase->captureCleanups==0&&currentCase->dispatch==1);
  assert(gameentry::active(gameentry::Client::Assist));++currentCase->resumed;
  return EXCEPTION_CONTINUE_EXECUTION;
 }
 return EXCEPTION_CONTINUE_SEARCH;
}
extern "C" void FixtureRuntimeScopeBody(void* raw){
 auto& state=*static_cast<Case*>(raw);
 const bool acquired=gameentry::scoped(gameentry::Client::Assist,true,[&]{
  ++state.entered;assert(gameentry::active(gameentry::Client::Assist));
  // Rejected scheduling must not release the live caller's exact lease.
  bool ran=false;const bool nested=gameentry::scoped(gameentry::Client::Cursor,false,[&]{ran=true;});
  assert(!nested&&!ran&&gameentry::active(gameentry::Client::Assist));
  assert(InterlockedCompareExchange(&state.dispatch,1,0)==0);
  // A losing CAS owns no cleanup. Rejection must retain the winner's lock.
  bool loserOwned=InterlockedCompareExchange(&state.dispatch,1,0)==0;
  runtimeunwind::run([]{},[&]{if(loserOwned)InterlockedExchange(&state.dispatch,0);});
  assert(!loserOwned&&state.dispatch==1);
  SetLastError(0x3456);RuntimeFPScope fp;
  runtimeunwind::run([&]{
   // A nested Capture has its own LIFO token; finally must release it before
   // the surrounding scoped Enter can release the outer token.
   gameentry::Request capture{};assert(gameentry::capture(gameentry::Client::Assist,capture));
   runtimeunwind::run([&]{
    if(state.mode==1)*static_cast<volatile unsigned*>(state.noAccess)=7;
    if(state.mode==2){RaiseException(EXCEPTION_BREAKPOINT,0,0,nullptr);assert(state.resumed==1&&state.cleanups==0);}
   },[&]{gameentry::leave(capture);++state.captureCleanups;});
  },[&]{
   assert(state.dispatch==1&&state.captureCleanups==1);
   InterlockedExchange(&state.dispatch,0);++state.cleanups;fp.restore();
  });
  assert(GetLastError()==0x3456);
  // Explicit cleanup and the eventual ordinary destructor cannot restore
  // the saved state a second time after the caller has changed LastError.
  SetLastError(0x789a);fp.restore();assert(GetLastError()==0x789a);
 });
 assert(acquired);++state.completed;
}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 void* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);assert(page);
 void* handler=AddVectoredExceptionHandler(1,resumeDebug);assert(handler);
 const auto originalControl=controlWord();const auto originalMxcsr=mxcsr();
 const unsigned short expectedControl=static_cast<unsigned short>((originalControl&~0x0c00u)|0x0800u);
 const unsigned expectedMxcsr=(originalMxcsr&~0x6000u)|0x2000u;
 unsigned checks=0;
 for(unsigned mode=0;mode<3;++mode){
  controlWord(expectedControl);mxcsr(expectedMxcsr);
  Case state;state.mode=mode;state.noAccess=page;currentCase=&state;
  const unsigned caught=FixtureRuntimeScopeOuter(&state);
  assert(caught==(mode==1?1u:0u));
  assert(state.entered==1&&state.completed==(mode==1?0u:1u));
  assert(state.cleanups==1&&state.captureCleanups==1&&state.dispatch==0&&state.resumed==(mode==2?1u:0u));
  assert(controlWord()==expectedControl&&mxcsr()==expectedMxcsr);
  // Inspect the real manager after the foreign handler consumed the AV.
  const auto idle=gameentry::inspect();assert(!idle.activeModule&&gameentry::activeToken==0);
  bool reentered=false;assert(gameentry::scoped(gameentry::Client::Assist,true,[&]{reentered=true;}));assert(reentered);
  currentCase=nullptr;++checks;
 }
 controlWord(originalControl);mxcsr(originalMxcsr);
 assert(RemoveVectoredExceptionHandler(handler));assert(VirtualFree(page,0,MEM_RELEASE));
 std::printf("PASS Windows x86 production scoped entry/manager + SEH finally: %u normal/outer-AV/resumed-debug cases; exact nested capture, rejected entry/CAS, FP restore and reentry passed; no game process\n",checks);
}
