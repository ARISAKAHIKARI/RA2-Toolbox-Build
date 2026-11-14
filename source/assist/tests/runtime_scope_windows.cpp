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
extern "C" int GuardedReadObjectIdentity(unsigned,unsigned*);
static DWORD WINAPI foreignValidation(void*){
 for(unsigned i=0;i<10000;++i)if(gameentry::active(gameentry::Client::Assist))return 1;
 return 0;
}
static void identityBenchmark(unsigned object){
 constexpr unsigned samples=50000;volatile unsigned sink=0;
 LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
 for(unsigned round=0;round<3;++round){double times[2]{};
  for(unsigned pass=0;pass<2;++pass){const unsigned mode=round==1?1-pass:pass;LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
   for(unsigned n=0;n<samples;++n){unsigned id=0;bool ok=false;
    if(mode)ok=GuardedReadObjectIdentity(object,&id)!=0;
    else{SIZE_T got=0;ok=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(uintptr_t(object+0x10)),&id,4,&got)&&got==4;}
    assert(ok&&id==99);sink+=id;
   }
   QueryPerformanceCounter(&end);times[mode]=double(end.QuadPart-begin.QuadPart)*1000000/double(frequency.QuadPart)/samples;
  }
  std::printf("IDENTITY_BENCH {\"round\":%u,\"samples\":%u,\"rpm_us_per_read\":%.6f,\"local_us_per_read\":%.6f}\n",round,samples,times[0],times[1]);
 }
 assert(sink==6*samples*99);
}
static void identityReadChecks(){
 auto*pages=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));assert(pages);
 const unsigned object=unsigned(uintptr_t(pages));unsigned id=0;
 *reinterpret_cast<unsigned*>(pages+0x10)=77;SetLastError(0x6432);
 assert(GuardedReadObjectIdentity(object,&id)&&id==77&&GetLastError()==0x6432);
 *reinterpret_cast<unsigned*>(pages+0x10)=99;
 assert(GuardedReadObjectIdentity(object,&id)&&id==99);
 DWORD prior=0;assert(VirtualProtect(pages,8192,PAGE_NOACCESS,&prior));id=123;
 SetLastError(0x6432);assert(!GuardedReadObjectIdentity(object,&id)&&id==123&&GetLastError()==0x6432);
 assert(VirtualProtect(pages,8192,PAGE_READWRITE,&prior));
 assert(VirtualProtect(pages+4096,4096,PAGE_NOACCESS,&prior));id=123;
 SetLastError(0x6432);assert(!GuardedReadObjectIdentity(object+4096-18,&id)&&id==123&&GetLastError()==0x6432);
 assert(!GuardedReadObjectIdentity(0,&id)&&!GuardedReadObjectIdentity(0x80000000u-0x14u,&id)&&!GuardedReadObjectIdentity(object,nullptr));
 assert(GuardedReadObjectIdentity(object,&id)&&id==99);
 identityBenchmark(object);
 assert(VirtualFree(pages,0,MEM_RELEASE));id=123;SetLastError(0x6432);
 assert(!GuardedReadObjectIdentity(object,&id)&&id==123&&GetLastError()==0x6432);
 assert(gameentry::scoped(gameentry::Client::Assist,true,[]{
  for(unsigned n=0;n<100000;++n)assert(gameentry::active(gameentry::Client::Assist));
  assert(!gameentry::active(gameentry::Client::Cursor));
  HANDLE thread=CreateThread(nullptr,0,foreignValidation,nullptr,0,nullptr);assert(thread);
  assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);DWORD result=99;
  assert(GetExitCodeThread(thread,&result)&&result==0);assert(CloseHandle(thread));
  assert(gameentry::active(gameentry::Client::Assist));
 }));
 std::puts("PASS actual x86 identity reads: live UID changes, AV/freed/cross-page failure, LastError preservation, recovery; 100000 live validations and 10000 foreign-thread denials");
}
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
 std::puts("STAGE body-enter");
 const bool acquired=gameentry::scoped(gameentry::Client::Assist,true,[&]{
  std::puts("STAGE body-acquired");
  ++state.entered;assert(gameentry::active(gameentry::Client::Assist));
  // Rejected scheduling must not release the live caller's exact lease.
  bool ran=false;const bool nested=gameentry::scoped(gameentry::Client::Cursor,false,[&]{ran=true;});
  assert(!nested&&!ran&&gameentry::active(gameentry::Client::Assist));
  assert(InterlockedCompareExchange(&state.dispatch,1,0)==0);
  // A losing CAS owns no cleanup. Rejection must retain the winner's lock.
  bool loserOwned=InterlockedCompareExchange(&state.dispatch,1,0)==0;
  runtimeunwind::run([]{},[&]{if(loserOwned)InterlockedExchange(&state.dispatch,0);});
  assert(!loserOwned&&state.dispatch==1);
  std::puts("STAGE body-fp");SetLastError(0x3456);RuntimeFPScope fp;
  std::puts("STAGE body-fp-ok");
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
static LONG WINAPI traceUnhandled(EXCEPTION_POINTERS*p){
 std::fprintf(stderr,"UNHANDLED code=%08lx ip=%08lx sp=%08lx bp=%08lx\n",p->ExceptionRecord->ExceptionCode,p->ContextRecord->Eip,p->ContextRecord->Esp,p->ContextRecord->Ebp);std::fflush(stderr);
 return EXCEPTION_CONTINUE_SEARCH;
}
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);SetUnhandledExceptionFilter(traceUnhandled);
 std::puts("STAGE scope-start");
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
  std::printf("STAGE scope-outer %u\n",mode);
  const unsigned caught=FixtureRuntimeScopeOuter(&state);
  std::printf("STAGE scope-return %u caught=%u\n",mode,caught);
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
 std::puts("STAGE identity-start");identityReadChecks();
 std::printf("PASS Windows x86 production scoped entry/manager + SEH finally: %u normal/outer-AV/resumed-debug cases; exact nested capture, rejected entry/CAS, FP restore and reentry passed; no game process\n",checks);
}
