// This fixture includes the complete production Service. Only Win32 handles,
// exception delivery and native INT3/NOP instructions are controlled boundaries.
// It does not claim to execute Windows exception continuation or a game.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <stdexcept>
#include <functional>
#include <chrono>
using DWORD=uint32_t;using LONG=int32_t;using BOOL=int;using PVOID=void*;
using HMODULE=void*;using LPCWSTR=const wchar_t*;
#define __cdecl
#define CALLBACK
#define WINAPI
constexpr DWORD WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,WAIT_FAILED=~0u,INFINITE=~0u;
constexpr DWORD CONTEXT_CONTROL=0x10001,CONTEXT_DEBUG_REGISTERS=0x10010,CONTEXT_ALL=0x1003f;
constexpr DWORD EXCEPTION_BREAKPOINT=0x80000003,EXCEPTION_SINGLE_STEP=0x80000004;
constexpr LONG EXCEPTION_CONTINUE_SEARCH=0,EXCEPTION_CONTINUE_EXECUTION=-1,EXCEPTION_EXECUTE_HANDLER=1;
constexpr DWORD ERROR_NOT_SUPPORTED=50,ERROR_BUSY=170,ERROR_INVALID_DATA=13,ERROR_INVALID_PARAMETER=87,ERROR_OPERATION_ABORTED=995;
constexpr DWORD SYNCHRONIZE=0x100000,GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS=4,GET_MODULE_HANDLE_EX_FLAG_PIN=1;
constexpr BOOL FALSE=0;
struct CONTEXT {DWORD ContextFlags=CONTEXT_ALL,Dr0=0,Dr1=0,Dr2=0,Dr3=0,Dr6=0,Dr7=0,Eip=0,EFlags=0x202,Eax=0x12345678,Esp=0x98765432;};
struct EXCEPTION_RECORD {DWORD ExceptionCode=0;void*ExceptionAddress=nullptr;};
struct EXCEPTION_POINTERS {EXCEPTION_RECORD*ExceptionRecord;CONTEXT*ContextRecord;};
struct KernelThread {DWORD tid;bool dead=false;CONTEXT context{};};
struct Handle {KernelThread*thread;bool closed=false;};using HANDLE=Handle*;
using Handler=LONG(*)(EXCEPTION_POINTERS*);
static KernelThread initial{101};static KernelThread*current=&initial;
static Handler handler=nullptr;static DWORD errorCode=5,ticks=100;
static bool failWait=false,failClose=false,failDuplicate=false,failAdd=false,failRemove=false,failPin=false;
static unsigned waitInfinite=0,duplicateCalls=0,closeCalls=0,trapCalls=0,witnessCalls=0,witnessHits=0;
static unsigned writes=0,callbacks=0,checks=0,scenarios=0;
static unsigned scopeCalls=0,scopeCleanups=0,foreignBypassCases=0,slotMatrixDeliveries=0;
static unsigned clockCalls=0,atomicCalls=0,threadIdCalls=0;
static unsigned productionSuspends=0,productionResumes=0,productionWorkers=0;
static std::vector<Handle*> handles;
static std::vector<const char*> names;
static void check(bool pass,const char*text,int line){++checks;if(!pass){std::fprintf(stderr,"FAIL line %d: %s (scenario %s)\n",line,text,names.empty()?"none":names.back());std::exit(1);}}
#define CHECK(x) check(bool(x),#x,__LINE__)
static void scenario(const char*name){++scenarios;names.push_back(name);}
__attribute__((noinline)) DWORD GetCurrentThreadId(){++threadIdCalls;return current->tid;}
DWORD GetTickCount(){++clockCalls;return ticks;}
DWORD GetLastError(){return errorCode;}
HANDLE GetCurrentProcess(){return reinterpret_cast<HANDLE>(1);}
HANDLE GetCurrentThread(){return reinterpret_cast<HANDLE>(2);}
BOOL DuplicateHandle(HANDLE,HANDLE,HANDLE,HANDLE*out,DWORD access,BOOL,DWORD){++duplicateCalls;CHECK(access==SYNCHRONIZE);if(failDuplicate)return 0;auto*h=new Handle{current};handles.push_back(h);*out=h;return 1;}
DWORD WaitForSingleObject(HANDLE h,DWORD timeout){if(timeout==INFINITE){++waitInfinite;return WAIT_FAILED;}CHECK(timeout==0);if(failWait||!h||h->closed)return WAIT_FAILED;return h->thread->dead?WAIT_OBJECT_0:WAIT_TIMEOUT;}
BOOL CloseHandle(HANDLE h){++closeCalls;if(failClose)return 0;CHECK(h&&!h->closed);h->closed=true;return 1;}
BOOL GetModuleHandleExW(DWORD,LPCWSTR,HMODULE*out){if(failPin)return 0;*out=reinterpret_cast<void*>(1);return 1;}
PVOID AddVectoredExceptionHandler(unsigned,Handler fn){if(failAdd)return nullptr;CHECK(!handler);handler=fn;return reinterpret_cast<void*>(3);}
unsigned RemoveVectoredExceptionHandler(PVOID){if(failRemove)return 0;handler=nullptr;return 1;}
LONG InterlockedExchange(volatile LONG*p,LONG v){++atomicCalls;LONG old=*p;*p=v;return old;}
LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG expected){++atomicCalls;LONG old=*p;if(old==expected)*p=v;return old;}
void MemoryBarrier(){}
struct BoundaryException {EXCEPTION_RECORD record;CONTEXT context;};
extern "C" __attribute__((noinline)) void RuntimeScopedCall(void*context,void(*body)(void*),void(*cleanup)(void*)){++scopeCalls;try{body(context);}catch(...){++scopeCleanups;cleanup(context);throw;}++scopeCleanups;cleanup(context);}
extern "C" int RuntimeBreakpointCall(void*context,void(*body)(void*),int(*filter)(void*,void*)){try{body(context);return 1;}catch(BoundaryException&ex){EXCEPTION_POINTERS p{&ex.record,&ex.context};if(filter(context,&p)==EXCEPTION_EXECUTE_HANDLER)return 0;throw;}}
#define private public
#include "production_core.h"
#undef private
using namespace assist::techbp;
enum Fault {None,Swallow,NoDebug,NoControl,BadIp,IgnoreWrite,ForeignException};
static std::map<unsigned,Fault> faults;
static bool allMissing=false,ignoreWrites=false,omitWitness=false,mutateBeforeWrite=false;
static unsigned trapEipOffset=0;static std::function<void()> beforeReply;
static std::function<void(CONTEXT&,uint32_t)> callbackAction;
static bool callbackResult=true;
static Service*active=nullptr;
static void commit(const CONTEXT&c){current->context.Dr0=c.Dr0;current->context.Dr1=c.Dr1;current->context.Dr2=c.Dr2;current->context.Dr3=c.Dr3;current->context.Dr6=c.Dr6;current->context.Dr7=c.Dr7;}
namespace assist { namespace techbp {
static void ownerRendezvousTrap(){
 ++trapCalls;Fault fault=faults.count(trapCalls)?faults[trapCalls]:None;if(allMissing)fault=NoDebug;
 if(fault==Swallow)return;
 const bool writing=active&&active->request_.mode==Service::WriteContext;
 if(writing&&mutateBeforeWrite){current->context.Dr3=0xfedc;current->context.Dr7|=enabled(3);mutateBeforeWrite=false;}
 CONTEXT context=current->context;context.ContextFlags=CONTEXT_ALL;context.Eip=Service::trapAddress()+trapEipOffset;
 if(fault==NoDebug)context.ContextFlags&=~0x10u;
 if(fault==NoControl)context.ContextFlags&=~1u;
 if(fault==BadIp)context.Eip+=9;
 EXCEPTION_RECORD record{fault==ForeignException?0xc0000005u:EXCEPTION_BREAKPOINT,reinterpret_cast<void*>(uintptr_t(Service::trapAddress()))};
 EXCEPTION_POINTERS p{&record,&context};const DWORD savedFlags=context.ContextFlags,savedEax=context.Eax,savedEsp=context.Esp;
 if(beforeReply)beforeReply();
 LONG result=handler?handler(&p):EXCEPTION_CONTINUE_SEARCH;
 CHECK(context.ContextFlags==savedFlags&&context.Eax==savedEax&&context.Esp==savedEsp);
 if(result==EXCEPTION_CONTINUE_SEARCH)throw BoundaryException{record,context};
 CHECK(context.Eip==Service::trapAddress()+1);
 if(writing&&active->request_.writeIssued)++writes;
 if(!(writing&&(fault==IgnoreWrite||ignoreWrites)))commit(context);
}
static void ownerRendezvousWitness(){
 ++witnessCalls;if(omitWitness)return;
 CONTEXT context=current->context;context.ContextFlags=CONTEXT_ALL;context.Eip=Service::witnessAddress();context.Dr6=0;
 const Registers r=Service::registers(context);for(unsigned n=0;n<4;++n)if(r.address[n]==context.Eip&&(r.control&slotMask(n))==enabled(n))context.Dr6|=1u<<n;
 if(!context.Dr6)return;
 EXCEPTION_RECORD record{EXCEPTION_SINGLE_STEP,reinterpret_cast<void*>(uintptr_t(context.Eip))};EXCEPTION_POINTERS p{&record,&context};
 CHECK(handler&&handler(&p)==EXCEPTION_CONTINUE_EXECUTION);CHECK(context.EFlags&0x10000u);++witnessHits;commit(context);
}
}}
static bool callback(void*raw,uint32_t tag){++callbacks;if(callbackAction)callbackAction(*static_cast<CONTEXT*>(raw),tag);return callbackResult;}
static void hit(uint32_t address){
 CONTEXT context=current->context;context.ContextFlags=CONTEXT_ALL;context.Eip=address;context.Dr6=0;
 Registers r=Service::registers(context);for(unsigned n=0;n<4;++n)if(r.address[n]==address&&(r.control&slotMask(n))==enabled(n))context.Dr6|=1u<<n;
 if(!context.Dr6)return;
 EXCEPTION_RECORD record{EXCEPTION_SINGLE_STEP,reinterpret_cast<void*>(uintptr_t(address))};EXCEPTION_POINTERS p{&record,&context};
 CHECK(handler&&handler(&p)==EXCEPTION_CONTINUE_EXECUTION);commit(context);
}
static uint32_t sites[SiteCount]={0x401000,0x402000,0x403000,0x404000,0x405000,0x406000,0x407000,0x408000,0x409000,0x40a000};
static void clearFaults(){faults.clear();allMissing=ignoreWrites=omitWitness=mutateBeforeWrite=false;failWait=failClose=failDuplicate=failAdd=failRemove=failPin=false;beforeReply={};callbackAction={};callbackResult=true;trapEipOffset=0;}
struct Case {
 KernelThread owner{101};Service service;
 explicit Case(const char*name){scenario(name);CHECK(!handler&&!Service::instance());clearFaults();current=&owner;active=&service;}
 ~Case(){clearFaults();current=service.thread_?service.thread_->thread:&owner;for(unsigned i=0;i<8&&service.resources();++i)service.retire();CHECK(!service.resources()&&!handler&&!Service::instance());active=nullptr;current=&initial;}
 bool enable(uint32_t wanted=3){return service.configure(sites,wanted,callback);}
};
static void setDebugAddress(CONTEXT&context,unsigned slot,uint32_t address){
 switch(slot){case 0:context.Dr0=address;break;case 1:context.Dr1=address;break;case 2:context.Dr2=address;break;case 3:context.Dr3=address;break;default:CHECK(false);}
}
// Exercise the real receiver over every physical slot. The expected decision
// follows the architectural local-execute encoding, without using slotMask()
// or enabled() to manufacture the incoming CONTEXT or the acceptance oracle.
static void slotMatrix(){
 for(unsigned slot=0;slot<4;++slot){
  Case c("physical-slot DR7, mixed DR6, uncertain ownership and TF/RF matrix");
  const uint32_t bit=1u<<slot,field=(3u<<(slot*2))|(15u<<(16+slot*4));
  for(unsigned foreign=0;foreign<4;++foreign)if(foreign!=slot){
   setDebugAddress(c.owner.context,foreign,0x710000u+foreign*0x100u);
   c.owner.context.Dr7|=(foreign&1u?2u:1u)<<(foreign*2);
  }
  const CONTEXT foreignBefore=c.owner.context;
  CHECK(c.enable(16)&&c.service.state==Ready&&c.service.lease.mask==bit);
  const CONTEXT installed=c.owner.context;const Lease lease=c.service.lease;
  auto verify=[&](CONTEXT context,bool owned,bool invoked,unsigned tag){
   ++slotMatrixDeliveries;
   CONTEXT expected=context;if(owned){expected.Dr6&=~bit;expected.EFlags|=0x10000u;}
   const unsigned scopes=scopeCalls,cleanups=scopeCleanups,cb=callbacks,clocks=clockCalls,atomics=atomicCalls,traps=trapCalls,tids=threadIdCalls;
   const LONG depth=c.service.handlerDepth_,exceptions=c.service.exceptions,changed=c.service.changed;
   callbackAction=[&](CONTEXT&,uint32_t actual){CHECK(actual==tag&&c.service.handlerDepth_==depth+1);};
   EXCEPTION_RECORD record{EXCEPTION_SINGLE_STEP,reinterpret_cast<void*>(uintptr_t(context.Eip))};EXCEPTION_POINTERS p{&record,&context};
   CHECK(Service::vectored(&p)==(owned?EXCEPTION_CONTINUE_EXECUTION:EXCEPTION_CONTINUE_SEARCH));
   CHECK(!std::memcmp(&context,&expected,sizeof context));
   CHECK(scopeCalls==scopes+unsigned(owned)&&scopeCleanups==cleanups+unsigned(owned)&&callbacks==cb+unsigned(invoked));
   CHECK(c.service.handlerDepth_==depth&&c.service.exceptions==exceptions+owned&&c.service.changed==changed+invoked);
   CHECK(clockCalls==clocks&&atomicCalls==atomics&&trapCalls==traps&&threadIdCalls==tids+1);
   callbackAction={};
  };
  for(unsigned enables=0;enables<4;++enables)for(unsigned rwlen=0;rwlen<16;++rwlen)for(unsigned flags=0;flags<4;++flags){
   CONTEXT context=installed;context.Eip=sites[4];context.Dr6=bit|0xffff0ff0u;
   context.Dr7=(context.Dr7&~field)|(enables<<(slot*2))|(rwlen<<(16+slot*4));
   context.EFlags=0x40202u|((flags&1u)?0x100u:0u)|((flags&2u)?0x10000u:0u);
   const bool owned=enables==1&&rwlen==0;verify(context,owned,owned,4);
  }
  for(unsigned pending=0;pending<16;++pending)for(unsigned causes=0;causes<8;++causes){
   CONTEXT context=installed;context.Eip=sites[4];context.EFlags=0x50302u;context.Dr6=0xffff0ff0u|pending|(causes<<13);
   const bool owned=pending==bit&&causes==0;verify(context,owned,owned,4);
  }
  CONTEXT context=installed;context.Eip=sites[4];context.Dr6=bit;
  setDebugAddress(context,slot,sites[4]+1);verify(context,false,false,4);
  context=installed;context.Dr6=bit;context.Eip=sites[4]+1;verify(context,false,false,4);
  context.Eip=sites[4];context.ContextFlags&=~0x10u;verify(context,false,false,4);
  c.service.lease.uncertain=bit;c.service.lease.alternative[slot]=0;c.service.lease.sites[0]=sites[0];c.service.confirmed=0;
  context=installed;context.Dr6=bit;context.Eip=sites[4];verify(context,true,false,4);
  setDebugAddress(context,slot,sites[0]);context.Eip=sites[0];verify(context,true,false,0);
  setDebugAddress(context,slot,sites[5]);context.Eip=sites[5];verify(context,false,false,5);
  setDebugAddress(context,slot,sites[0]);context.Eip=sites[4];verify(context,false,false,0);
  c.service.lease.uncertain=0;context.Eip=sites[0];verify(context,false,false,0);
  c.service.lease=lease;c.service.confirmed=1;
  CHECK(!c.service.retire()&&!c.service.occupied());
  CHECK(Service::same(Service::registers(c.owner.context),Service::registers(foreignBefore)));
 }
}
// Time the complete production Service entry/dispatch/cleanup, retaining an
// out-of-line boundary as in the real binary. The Win32/SEH boundary and final
// feature callback are controlled; these are not Windows DR/VEH timings.
static int benchmark(unsigned iterations){
 CHECK(iterations>0&&iterations<=20000000u);
 Case c("owned production Service benchmark");CHECK(c.enable(16));
 CONTEXT context=c.owner.context;context.Eip=sites[4];
 EXCEPTION_RECORD record{EXCEPTION_SINGLE_STEP,reinterpret_cast<void*>(uintptr_t(context.Eip))};EXCEPTION_POINTERS p{&record,&context};
 const uint32_t slots=c.service.lease.mask;const Handler receive=handler;
 const unsigned scopes=scopeCalls,cleanups=scopeCleanups,cb=callbacks,clocks=clockCalls,atomics=atomicCalls,tids=threadIdCalls,traps=trapCalls;
 const LONG exceptions=c.service.exceptions,changed=c.service.changed;
 const auto started=std::chrono::steady_clock::now();
 for(unsigned i=0;i<iterations;++i){context.Dr6=slots;receive(&p);}
 const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
 CHECK(scopeCalls-scopes==iterations&&scopeCleanups-cleanups==iterations&&callbacks-cb==iterations);
 CHECK(uint32_t(c.service.exceptions-exceptions)==iterations&&uint32_t(c.service.changed-changed)==iterations&&!c.service.handlerDepth_);
 CHECK(context.Dr6==0&&(context.EFlags&0x10000u)&&clockCalls==clocks&&atomicCalls==atomics&&trapCalls==traps&&threadIdCalls-tids==iterations);
 std::printf("{\"iterations\":%u,\"elapsed_ns\":%lld,\"scopes\":%u,\"cleanups\":%u,\"callbacks\":%u,\"thread_id_calls\":%u,\"clock_calls\":%u,\"atomic_calls\":%u,\"maintenance_traps\":%u,\"windows_executed\":false}\n",iterations,static_cast<long long>(elapsed),scopeCalls-scopes,scopeCleanups-cleanups,callbacks-cb,threadIdCalls-tids,clockCalls-clocks,atomicCalls-atomics,trapCalls-traps);
 return 0;
}
int main(int argc,char**argv){
 if(argc==3&&!std::strcmp(argv[1],"--benchmark"))return benchmark(unsigned(std::strtoul(argv[2],nullptr,10)));
 {Case c("normal witness, readback, synchronous install and owned cleanup");const auto original=c.owner.context;unsigned w=witnessHits;CHECK(c.enable());CHECK(c.service.state==Ready&&c.service.confirmed&&c.service.activeTags()==3);CHECK(witnessHits==w+1&&c.service.witnessed_&&!c.service.witnessLease_.mask);CHECK(!c.service.pending()&&!c.service.handlerDepth_);unsigned cb=callbacks;hit(sites[0]);hit(sites[1]);CHECK(callbacks==cb+2);CHECK(!c.service.retire());CHECK(!c.service.occupied());CHECK(Service::same(Service::registers(c.owner.context),Service::registers(original)));}
 {Case c("private three-slot group shares and releases with control");CHECK(c.enable(16));CHECK(c.enable(16|896));CHECK(c.service.state==Ready&&c.service.activeTags()==(16|896));unsigned cb=callbacks;hit(sites[7]);hit(sites[8]);hit(sites[9]);CHECK(callbacks==cb+3);CHECK(c.enable(16)&&c.service.activeTags()==16);CHECK(assist::slotbudget::tags(c.service.slotBudget())==16);}
 {Case c("private group shortage is transactional");CHECK(c.enable(3));const auto before=c.owner.context;CHECK(c.enable(3|896));CHECK(c.service.state==Slots&&c.service.activeTags()==3);CHECK(Service::same(Service::registers(before),Service::registers(c.owner.context)));}
 {Case c("INT3 EIP at trap plus one");trapEipOffset=1;CHECK(c.enable(1)&&c.service.state==Ready);}
 for(Fault fault:{Swallow,NoDebug,NoControl,BadIp}){Case c("missing or consumed private trap rejects locally and retries");faults[trapCalls+1]=fault;CHECK(c.enable(1));CHECK(c.service.state==System&&c.service.lastError==ERROR_NOT_SUPPORTED&&!c.service.occupied());CHECK(!c.service.pending()&&!c.service.handlerDepth_);faults.clear();CHECK(c.enable(1)&&c.service.state==Ready);}
 {Case c("self-test hardware witness absent restores first and rejects");omitWitness=true;CHECK(c.enable(1));CHECK(c.service.state==System&&c.service.lastError==ERROR_NOT_SUPPORTED&&!c.service.occupied()&&!c.owner.context.Dr0);omitWitness=false;CHECK(c.enable(1)&&c.service.state==Ready);}
 {Case c("continuation register write ignored is not confirmed");ignoreWrites=true;CHECK(c.enable(1));CHECK(c.service.state==System&&!c.service.activeTags()&&!c.service.occupied()&&!c.owner.context.Dr0);ignoreWrites=false;CHECK(c.enable(1)&&c.service.state==Ready);}
 {Case c("four foreign addresses remain unmodified");c.owner.context.Dr0=0xa0;c.owner.context.Dr1=0xa1;c.owner.context.Dr2=0xa2;c.owner.context.Dr3=0xa3;c.owner.context.Dr7=enabled(0);auto before=c.owner.context;unsigned w=witnessCalls;CHECK(c.enable(1));CHECK(c.service.state==Slots&&!c.service.occupied()&&witnessCalls==w);CHECK(Service::same(Service::registers(before),Service::registers(c.owner.context)));}
 {Case c("foreign live slot survives witness install and retirement");c.owner.context.Dr0=0xabcd;c.owner.context.Dr7=enabled(0);CHECK(c.enable()&&c.service.state==Ready&&c.service.lease.mask==6);CHECK(c.owner.context.Dr0==0xabcd);CHECK(!c.service.retire());CHECK(c.owner.context.Dr0==0xabcd&&c.owner.context.Dr7==enabled(0));}
 {Case c("slot shortage preserves healthy sibling callback");CHECK(c.enable(1));c.owner.context.Dr1=0xb1;c.owner.context.Dr2=0xb2;c.owner.context.Dr3=0xb3;CHECK(c.enable(3));CHECK(c.service.state==Slots&&c.service.activeTags()==1);unsigned cb=callbacks;hit(sites[0]);CHECK(callbacks==cb+1);}
 {Case c("readback and bounded cleanup failure retain possible lease");CHECK(c.enable(1));const unsigned start=trapCalls;faults[start+3]=NoDebug;faults[start+4]=NoDebug;CHECK(!c.service.retire());CHECK(c.service.state==System&&c.service.lease.mask&&c.service.handler_&&!c.service.confirmed);CHECK(trapCalls==start+4&&!c.service.pending());faults.clear();CHECK(!c.service.retire());CHECK(!c.service.lease.mask);}
 {Case c("ignored restore disables callback until exact cleanup");CHECK(c.enable(1));ignoreWrites=true;unsigned cb=callbacks;CHECK(!c.service.retire());CHECK(c.service.state==System&&c.service.occupied());hit(sites[0]);CHECK(callbacks==cb);ignoreWrites=false;CHECK(!c.service.retire()&&!c.service.occupied());}
 {Case c("witness cleanup failure is pending ownership without background job");unsigned start=trapCalls;faults[start+5]=IgnoreWrite;CHECK(c.enable(1));CHECK(c.service.state==System&&c.service.witnessLease_.mask&&!c.service.lease.mask);CHECK(c.service.pendingFor(AllTags)&&!c.service.pending());faults.clear();CHECK(!c.service.retire());CHECK(!c.service.witnessLease_.mask);CHECK(c.enable(1)&&c.service.state==Ready);}
 {Case c("foreign write between sample and commit is preserved");mutateBeforeWrite=true;CHECK(c.enable(1));CHECK(c.service.state==Foreign&&!c.service.occupied());CHECK(c.owner.context.Dr3==0xfedc&&(c.owner.context.Dr7&enabled(3)));}
 {Case c("foreign replacement removes only still-owned resources");CHECK(c.enable());c.owner.context.Dr0=0xabba;CHECK(c.service.audit());CHECK(c.service.state==Foreign&&!c.service.occupied());CHECK(c.owner.context.Dr0==0xabba&&!c.owner.context.Dr1);}
 {Case c("callback rejects every nested configuration and suppresses nested callback");CHECK(c.enable());unsigned start=trapCalls,cb=callbacks;callbackAction=[&](CONTEXT&,uint32_t){CHECK(c.service.handlerDepth_==1);CHECK(!c.service.configure(sites,1,callback));CHECK(!c.service.enable(sites[0],sites[1],callback));CHECK(!c.service.audit()&&!c.service.inspect()&&!c.service.retire());hit(sites[1]);CHECK(c.service.handlerDepth_==1);};hit(sites[0]);CHECK(callbacks==cb+1&&trapCalls==start&&!c.service.handlerDepth_&&c.service.activeTags()==3);callbackAction={};}
 {Case c("exception propagation clears handler depth and transaction job");CHECK(c.enable(1));callbackAction=[](CONTEXT&,uint32_t){throw std::runtime_error("callback");};bool caught=false;try{hit(sites[0]);}catch(const std::runtime_error&){caught=true;}CHECK(caught&&!c.service.handlerDepth_&&!c.service.pending());callbackAction={};faults[trapCalls+1]=ForeignException;caught=false;try{c.service.audit();}catch(const BoundaryException&){caught=true;}CHECK(caught&&!c.service.pending()&&!c.service.handlerDepth_&&!c.service.request_.token&&c.service.state==System);faults.clear();}
 {Case c("mixed and non-owned hardware exceptions are unchanged");CHECK(c.enable(1));CONTEXT context=c.owner.context;context.Eip=sites[0];context.Dr6=1|(1u<<14);CONTEXT before=context;EXCEPTION_RECORD record{EXCEPTION_SINGLE_STEP,reinterpret_cast<void*>(uintptr_t(sites[0]))};EXCEPTION_POINTERS p{&record,&context};CHECK(Service::vectored(&p)==EXCEPTION_CONTINUE_SEARCH&&!std::memcmp(&context,&before,sizeof context));context.Dr6=8;before=context;CHECK(Service::vectored(&p)==EXCEPTION_CONTINUE_SEARCH&&!std::memcmp(&context,&before,sizeof context));}
 {Case c("foreign exceptions bypass owner bookkeeping and SEH scope");CHECK(c.enable(1));
  auto foreign=[&](DWORD code,unsigned variation){
   CONTEXT context=c.owner.context;context.Eip=sites[0];context.Dr6=1;
   if(variation==1)context.ContextFlags&=~0x10u;
   else if(variation==2)context.Eip+=1;
   else if(variation==3)context.Dr0+=4;
   else if(variation==4)context.Dr7=2;
   else if(variation==5)context.Dr7|=1u<<16;
   else if(variation==6)context.Dr6=0;
   else if(variation>=7)context.Dr6|=variation==7?2u:variation==8?4u:variation==9?8u:1u<<(variation+3);
   const CONTEXT before=context;EXCEPTION_RECORD record{code,reinterpret_cast<void*>(uintptr_t(context.Eip))};EXCEPTION_POINTERS p{&record,&context};
   const unsigned scopes=scopeCalls,cb=callbacks;const LONG hits=c.service.exceptions,changed=c.service.changed,depth=c.service.handlerDepth_;
   CHECK(Service::vectored(&p)==EXCEPTION_CONTINUE_SEARCH);CHECK(!std::memcmp(&context,&before,sizeof context));
   CHECK(scopeCalls==scopes&&callbacks==cb&&c.service.exceptions==hits&&c.service.changed==changed&&c.service.handlerDepth_==depth);++foreignBypassCases;
  };
  foreign(0xc0000005u,0);foreign(0xc0000006u,0);foreign(EXCEPTION_BREAKPOINT,0);
  for(unsigned v=1;v<=12;++v)foreign(EXCEPTION_SINGLE_STEP,v);
  // A local guarded AV raised inside a feature callback is foreign to the
  // DR service too. It must not construct another scope or perturb depth.
  callbackAction=[&](CONTEXT&,uint32_t){foreign(0xc0000005u,0);};hit(sites[0]);callbackAction={};
 }
 {Case c("live wrong-thread calls preserve healthy owner");CHECK(c.enable(1));KernelThread other{202};current=&other;unsigned start=trapCalls;CHECK(!c.service.audit()&&!c.service.configure(sites,1,callback)&&!c.service.retire());CHECK(c.service.state==Ready&&c.service.activeTags()==1&&trapCalls==start);current=&c.owner;}
 for(DWORD id:{101u,202u}){Case c("dead owner rebinds real kernel object even for same ID and same wanted");CHECK(c.enable(1));HANDLE old=c.service.thread_;unsigned w=witnessHits;c.owner.dead=true;KernelThread next{id};current=&next;CHECK(c.enable(1));CHECK(c.service.thread_!=old&&old->closed&&c.service.owner()==id&&c.service.thread_->thread==&next);CHECK(c.service.state==Ready&&witnessHits==w+1&&next.context.Dr0==sites[0]);CHECK(!c.service.retire());CHECK(c.service.retire());}
 {Case c("WAIT_FAILED preserves identity and lease instead of declaring exit");CHECK(c.enable(1));HANDLE old=c.service.thread_;auto mask=c.service.lease.mask;failWait=true;CHECK(!c.enable(1));CHECK(c.service.state==System&&c.service.thread_==old&&c.service.lease.mask==mask&&!old->closed);CHECK(!c.service.retire());failWait=false;CHECK(c.enable(1)&&c.service.state==Ready);}
 {Case c("same wanted with changed site never takes ready shortcut");CHECK(c.enable(1));uint32_t changed[SiteCount];std::memcpy(changed,sites,sizeof sites);changed[0]+=16;unsigned start=trapCalls;CHECK(c.service.configure(changed,1,callback));CHECK(trapCalls>start&&c.service.state==Foreign&&!c.service.activeTags());CHECK(c.enable(1)&&c.service.state==Ready);CHECK(!c.service.enable(sites[0]+16,sites[1],callback));CHECK(c.service.state==Foreign&&c.service.lease.sites[0]==sites[0]&&c.service.lease.mask);}
 {Case c("first zero-wanted inspect does not falsely mark witness passed");CHECK(c.enable(0));CHECK(!c.service.witnessed_&&!c.service.occupied());CHECK(c.service.inspect()&&c.service.state==Off&&c.service.slotBudget());CHECK(!c.service.witnessed_);CHECK(c.enable(1)&&c.service.witnessed_);}
 {Case c("remove handler and close handle failures retain cleanup responsibility");CHECK(c.service.inspect());failRemove=true;CHECK(!c.service.retire()&&c.service.handler_&&Service::instance()==&c.service);failRemove=false;failClose=true;CHECK(!c.service.retire()&&!c.service.handler_&&c.service.thread_);failClose=false;CHECK(c.service.retire()&&!c.service.resources());}
 {Case c("creation failures leave no phantom install and are retryable");failDuplicate=true;CHECK(!c.enable(1)&&!c.service.thread_);failDuplicate=false;failPin=true;CHECK(!c.enable(1)&&!c.service.handler_);failPin=false;failAdd=true;CHECK(!c.enable(1)&&!Service::instance());failAdd=false;CHECK(c.enable(1)&&c.service.state==Ready);}
 {Case c("token wraps without accepting zero or stale response");c.service.serial_=0xffffffffu;CHECK(c.enable(1)&&c.service.state==Ready&&c.service.serial_&&!c.service.request_.token);}
 {Case c("uncertain A or B resolves to B before attempted C is ignored");CHECK(c.enable(1));auto&l=c.service.lease;l.sites[1]=sites[1];l.sites[2]=sites[2];l.alternative[0]=1;l.uncertain=1;c.owner.context.Dr0=sites[1];Registers before=Service::registers(c.owner.context),after=before;Lease proposed=l;CHECK(proposed.configure(after,sites,4)==Ready);ignoreWrites=true;c.service.job_=1;int result=c.service.transfer(l,proposed,before,after,Ready,false);c.service.job_=0;CHECK(result==System&&l.mask==1&&l.tag[0]==1&&!l.uncertain&&l.sites[l.tag[0]]==sites[1]);ignoreWrites=false;}
 {Case c("audit resolves uncertain lease before confirming");CHECK(c.enable(1));auto&l=c.service.lease;l.sites[1]=sites[1];l.alternative[0]=1;l.uncertain=1;c.owner.context.Dr0=sites[1];CHECK(c.service.audit());CHECK(c.service.state==Ready&&!l.uncertain&&l.tag[0]==1&&c.service.activeTags()==2);}
 for(unsigned mode=0;mode<12;++mode){Case c("owned dispatch preserves scope, fallback, TF/RF and counters");CHECK(c.enable(16));
  const LONG states[]={Ready,Slots,Signature,Waiting,Off,System,Foreign,Thread};
  if(mode<8)c.service.state=states[mode];else if(mode==8)c.service.confirmed=0;else if(mode==9)c.service.callback=nullptr;else if(mode==10)c.service.handlerDepth_=1;else callbackResult=false;
  CONTEXT context=c.owner.context;context.Eip=sites[4];context.Dr6=c.service.lease.mask|0xffff0ff0u;context.EFlags=0x40302u;
  CONTEXT expected=context;expected.Dr6&=~c.service.lease.mask;expected.EFlags|=0x10000u;
  EXCEPTION_RECORD record{EXCEPTION_SINGLE_STEP,reinterpret_cast<void*>(uintptr_t(context.Eip))};EXCEPTION_POINTERS p{&record,&context};
  const unsigned scopes=scopeCalls,cleanups=scopeCleanups,cb=callbacks,clocks=clockCalls,atomics=atomicCalls,traps=trapCalls,tids=threadIdCalls;
  const LONG depth=c.service.handlerDepth_,exceptions=c.service.exceptions,changed=c.service.changed;
  callbackAction=[&](CONTEXT&,uint32_t tag){CHECK(tag==4&&c.service.handlerDepth_==1);};
  CHECK(Service::vectored(&p)==EXCEPTION_CONTINUE_EXECUTION);CHECK(!std::memcmp(&context,&expected,sizeof context));
  const bool invoked=mode<3||mode==11;
  CHECK(scopeCalls==scopes+1&&scopeCleanups==cleanups+1&&callbacks==cb+unsigned(invoked));
  CHECK(c.service.handlerDepth_==depth&&c.service.exceptions==exceptions+1&&c.service.changed==changed+(invoked&&callbackResult));
  CHECK(clockCalls==clocks&&atomicCalls==atomics&&trapCalls==traps&&threadIdCalls==tids+1);
  c.service.handlerDepth_=0;c.service.confirmed=1;c.service.state=Ready;c.service.callback=callback;
 }
 slotMatrix();
 CHECK(waitInfinite==0&&productionSuspends==0&&productionResumes==0&&productionWorkers==0);
 for(auto*h:handles){CHECK(h->closed);delete h;}
 std::printf("{\"pass\":true,\"scenarios\":%u,\"checks\":%u,\"trap_calls\":%u,\"witness_hits\":%u,\"foreign_bypass_cases\":%u,\"slot_matrix_deliveries\":%u,\"infinite_waits\":%u,\"production_suspend_calls\":%u,\"production_resume_calls\":%u,\"production_worker_calls\":%u,\"windows_executed\":false,\"game_executed\":false}\n",scenarios,checks,trapCalls,witnessHits,foreignBypassCases,slotMatrixDeliveries,waitInfinite,productionSuspends,productionResumes,productionWorkers);
}

