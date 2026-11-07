#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cassert>
#include <cstring>
// Inject only the synchronous private-call boundary; normal paths execute the
// actual production INT3, VEH, witness and native CONTEXT continuation.
static volatile LONG contextReads=0,failReadAt=0,skipSetOnce=0;
#define RuntimeBreakpointCall fixtureBreakpointCall
#define private public
#include "../source/tech_ui_breakpoint_core.h"
#undef private
#undef RuntimeBreakpointCall
extern "C" int __cdecl RuntimeBreakpointCall(void*,void(__cdecl*)(void*),int(__cdecl*)(void*,void*));
extern "C" int __cdecl fixtureBreakpointCall(void*raw,void(__cdecl*body)(void*),int(__cdecl*filter)(void*,void*)){
 auto&s=*static_cast<assist::techbp::Service*>(raw);
 if(s.request_.mode==assist::techbp::Service::ReadContext){const LONG n=InterlockedIncrement(&contextReads);if(failReadAt&&n>=failReadAt){SetLastError(ERROR_NOT_SUPPORTED);return 0;}}
 if(s.request_.mode==assist::techbp::Service::WriteContext&&skipSetOnce>0){InterlockedDecrement(&skipSetOnce);s.request_.reply=s.request_.token;s.request_.valid=true;s.request_.observed=s.request_.before;s.request_.writeIssued=true;return 1;}
 return RuntimeBreakpointCall(raw,body,filter);
}
#ifdef NDEBUG
#error Hardware breakpoint verification requires -UNDEBUG.
#endif
struct House{int tag;};struct Type{int tag;};
static House local{7},other{9};static Type allowed{11},denied{13};
static volatile uint32_t returned=0;static volatile LONG passedForeign=0;
extern "C" __declspec(noinline) int __thiscall SampleCanBuild(House*,Type*,bool limit,bool production){returned=uint32_t(uintptr_t(__builtin_return_address(0)));return int(limit)+2*int(production);}
extern "C" __declspec(naked) int __stdcall DrawCaller(House*,Type*){__asm {
 mov ecx,[esp+4]
 mov eax,[esp+8]
 push 0
 push 0
 push eax
 call SampleCanBuild
 ret 8
}}
extern "C" __declspec(naked) int __stdcall PruneCaller(House*,Type*){__asm {
 mov ecx,[esp+4]
 mov eax,[esp+8]
 push 1
 push 0
 push eax
 call SampleCanBuild
 ret 8
}}
static bool apply(void*raw,uint32_t){
 auto&c=*static_cast<CONTEXT*>(raw);auto*args=reinterpret_cast<uint32_t*>(uintptr_t(c.Esp));
 if(c.Ecx!=uint32_t(uintptr_t(&local))||args[0]!=uint32_t(uintptr_t(&allowed)))return false;
 args[1]=1;return true;
}
static LONG WINAPI foreignHandler(EXCEPTION_POINTERS*p){if(p->ExceptionRecord->ExceptionCode==EXCEPTION_SINGLE_STEP){InterlockedIncrement(&passedForeign);return EXCEPTION_CONTINUE_EXECUTION;}return EXCEPTION_CONTINUE_SEARCH;}
static void wait(assist::techbp::Service&s){DWORD start=GetTickCount();while(s.pending()){assert(GetTickCount()-start<5000);Sleep(1);} }
static void retire(assist::techbp::Service&s){DWORD start=GetTickCount();while(!s.retire()){assert(GetTickCount()-start<5000);Sleep(1);}}
struct DebugAccess{HANDLE target;CONTEXT value{};bool write=false,ok=false;};
static DWORD WINAPI debugWorker(void*raw){auto&job=*static_cast<DebugAccess*>(raw);DWORD prior=SuspendThread(job.target);if(prior==DWORD(-1))return 1;if(!prior){job.value.ContextFlags=CONTEXT_DEBUG_REGISTERS;job.ok=(job.write?SetThreadContext(job.target,&job.value):GetThreadContext(job.target,&job.value))!=FALSE;}assert(ResumeThread(job.target)!=DWORD(-1));return 0;}
static CONTEXT debug(CONTEXT*write=nullptr){DebugAccess job{};job.target=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,GetCurrentThreadId());assert(job.target);job.write=write!=nullptr;if(write)job.value=*write;HANDLE helper=CreateThread(nullptr,0,debugWorker,&job,0,nullptr);assert(helper);assert(WaitForSingleObject(helper,5000)==WAIT_OBJECT_0&&job.ok);CloseHandle(helper);CloseHandle(job.target);return job.value;}
extern "C" __declspec(noinline) void foreignMarker(){MemoryBarrier();}
static double benchmark(unsigned count,bool active){LARGE_INTEGER start{},end{},frequency{};assert(QueryPerformanceFrequency(&frequency));assert(QueryPerformanceCounter(&start));for(unsigned i=0;i<count;++i){assert(DrawCaller(&local,&allowed)==(active?1:0));assert(PruneCaller(&local,&allowed)==(active?3:2));}assert(QueryPerformanceCounter(&end));return double(end.QuadPart-start.QuadPart)*1000000.0/double(frequency.QuadPart);}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 auto foreign=AddVectoredExceptionHandler(0,foreignHandler);assert(foreign);
 assert(DrawCaller(&local,&allowed)==0);uint32_t a=returned-5;
 assert(PruneCaller(&local,&allowed)==2);uint32_t b=returned-5;
 assert(*reinterpret_cast<uint8_t*>(uintptr_t(a))==0xe8&&*reinterpret_cast<uint8_t*>(uintptr_t(b))==0xe8);
 uint8_t beforeA[32],beforeB[32];memcpy(beforeA,reinterpret_cast<void*>(uintptr_t(a-8)),32);memcpy(beforeB,reinterpret_cast<void*>(uintptr_t(b-8)),32);
 assist::techbp::Service service;
 CONTEXT original=debug();assert(!original.Dr0&&!original.Dr1&&!original.Dr2&&!original.Dr3&&!(original.Dr7&0xffff00ffu));
 const double baselineUs=benchmark(10000,false);
 assert(service.enable(a,b,apply));wait(service);
 printf("install=%ld error=%ld mask=%u\n",service.state,service.lastError,service.lease.mask);
 assert(service.state==assist::techbp::Ready&&service.occupied());
 assert(DrawCaller(&local,&allowed)==1);assert(PruneCaller(&local,&allowed)==3);
 assert(DrawCaller(&other,&allowed)==0);assert(PruneCaller(&local,&denied)==2);
 assert(service.changed==2&&service.exceptions>=4);
 // A simultaneous foreign single-step cause must see the exact original
 // exception CONTEXT; this deliberately exercises the real dispatcher body.
 CONTEXT mixed=debug();mixed.Eip=a;mixed.EFlags=0x202;mixed.Dr6=service.lease.mask|(1u<<14);CONTEXT untouched=mixed;
 EXCEPTION_RECORD record{};record.ExceptionCode=EXCEPTION_SINGLE_STEP;EXCEPTION_POINTERS pointers{&record,&mixed};
 assert(assist::techbp::Service::vectored(&pointers)==EXCEPTION_CONTINUE_SEARCH&&!memcmp(&mixed,&untouched,sizeof mixed));
 const LONG beforeHits=service.exceptions,beforeChanges=service.changed;
 const double activeUs=benchmark(10000,true);
 assert(service.exceptions-beforeHits==20000&&service.changed-beforeChanges==20000);
 printf("QPC calls_per_caller=10000 total_calls=20000 baseline_us=%.3f active_us=%.3f additional_us_per_exception=%.6f hits=%ld changes=%ld\n",baselineUs,activeUs,(activeUs-baselineUs)/20000.0,service.exceptions-beforeHits,service.changed-beforeChanges);
 RaiseException(EXCEPTION_SINGLE_STEP,0,0,nullptr);assert(passedForeign==1);
 assert(!memcmp(beforeA,reinterpret_cast<void*>(uintptr_t(a-8)),32)&&!memcmp(beforeB,reinterpret_cast<void*>(uintptr_t(b-8)),32));
 retire(service);assert(service.state==assist::techbp::Off&&!service.occupied());
 assert(DrawCaller(&local,&allowed)==0&&PruneCaller(&local,&allowed)==2);
 CONTEXT after=debug();assert(after.Dr0==original.Dr0&&after.Dr1==original.Dr1&&after.Dr2==original.Dr2&&after.Dr3==original.Dr3&&after.Dr7==original.Dr7);
 // A pre-existing enabled slot is retained; the lease uses the other slots.
 CONTEXT foreignSlot=original;foreignSlot.Dr0=DWORD(uintptr_t(&foreignMarker));foreignSlot.Dr7|=1;debug(&foreignSlot);
 assert(service.enable(a,b,apply));wait(service);assert(service.state==assist::techbp::Ready&&service.lease.mask==6);
 assert(DrawCaller(&local,&allowed)==1&&PruneCaller(&local,&allowed)==3);retire(service);
 after=debug();assert(after.Dr0==foreignSlot.Dr0&&after.Dr7==foreignSlot.Dr7&&!after.Dr1&&!after.Dr2&&!after.Dr3);
 // Even disabled nonzero foreign addresses are occupied and are never stolen.
 CONTEXT full=foreignSlot;full.Dr1=full.Dr2=full.Dr3=full.Dr0;debug(&full);
 assert(service.enable(a,b,apply));wait(service);assert(service.state==assist::techbp::Slots&&!service.occupied());retire(service);
 after=debug();assert(after.Dr0==full.Dr0&&after.Dr1==full.Dr1&&after.Dr2==full.Dr2&&after.Dr3==full.Dr3&&after.Dr7==full.Dr7);
 debug(&original);assert(service.enable(a,b,apply));wait(service);assert(service.lease.mask==3);
 // Another component replaces one owned slot. Retiring releases only the
 // still-matching slot and preserves the replacement address/control bits.
 CONTEXT replaced=debug();replaced.Dr0=DWORD(uintptr_t(&foreignMarker));debug(&replaced);
 assert(service.audit());wait(service);assert(service.state==assist::techbp::Foreign);retire(service);
 after=debug();assert(after.Dr0==replaced.Dr0&&(after.Dr7&assist::techbp::slotMask(0))==(replaced.Dr7&assist::techbp::slotMask(0))&&!after.Dr1);debug(&original);
 // True own-thread restore with an injected failed readback must retain the
 // lease/handler even though the kernel may already have removed the slots.
 assert(service.enable(a,b,apply));wait(service);assert(service.occupied());contextReads=0;failReadAt=2;
 assert(!service.retire());wait(service);assert(service.state==assist::techbp::System&&service.occupied()&&service.resources());
 failReadAt=0;retire(service);after=debug();assert(after.Dr0==original.Dr0&&after.Dr1==original.Dr1&&after.Dr7==original.Dr7);
 // A supplied continuation whose readback still contains our breakpoint
 // cannot release the VEH. Disable callbacks until an exact restore succeeds.
 assert(service.enable(a,b,apply));wait(service);skipSetOnce=2;
 assert(!service.retire());wait(service);assert(service.state==assist::techbp::System&&service.occupied());
 assert(DrawCaller(&local,&allowed)==0);retire(service);assert(DrawCaller(&local,&allowed)==0);
 // Logical FPS tag 5 shares the same four physical slots and ownership.
 uint32_t sites[assist::techbp::SiteCount]{};sites[0]=a;sites[5]=b;
 assert(service.configure(sites,33,apply));wait(service);assert(service.state==assist::techbp::Ready&&service.activeTags()==33);
 assert(DrawCaller(&local,&allowed)==1&&PruneCaller(&local,&allowed)==3);
 assert(assist::slotbudget::tags(service.slotBudget())==33);retire(service);
 after=debug();assert(after.Dr0==original.Dr0&&after.Dr1==original.Dr1&&after.Dr7==original.Dr7);
 assert(RemoveVectoredExceptionHandler(foreign));
 puts("PASS: real Windows x86 two CALL execution breakpoints; 20000 quantified hits; own-thread context install/readback/restore; argument-only change; House/type gates; foreign exception pass-through; foreign slots preserved; insufficient slots refused; foreign replacement preserved on restore; unchanged code; no game");
 return 0;
}
