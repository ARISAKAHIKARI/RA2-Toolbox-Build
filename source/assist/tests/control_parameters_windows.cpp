// Reuse the existing production Service and real Win32 context/fault helpers.
#define main baselineTechnologyFixtureMain
#include "tech_ui_breakpoint_windows.cpp"
#undef main
#include "../source/control_parameter_policy.h"
static volatile uint32_t predicateReturn=0;
static uint32_t ownerReturn=0,planningReturn=0;
static bool controlEnabled=true;
static uint32_t drawReturn=0,pruneReturn=0;
extern "C" __declspec(noinline) int __thiscall ControlPredicate(House*p){predicateReturn=uint32_t(uintptr_t(__builtin_return_address(0)));return p==&local;}
extern "C" __declspec(naked) int ControlAllowed(){__asm {
 mov eax,1
 ret
}}
extern "C" __declspec(naked) int ControlStrict(){__asm {
 lea edx,local
 xor eax,eax
 cmp ecx,edx
 sete al
 ret
}}
extern "C" __declspec(naked) int __stdcall PermissionCaller(House*){__asm {
 mov ecx,[esp+4]
 call ControlPredicate
 ret 4
}}
extern "C" __declspec(naked) int __stdcall OwnerCaller(House*){__asm {
 mov ecx,[esp+4]
 call ControlPredicate
 test al,al
 jz original
 lea eax,local
 mov eax,[eax]
 ret 4
original:
 mov eax,[esp+4]
 mov eax,[eax]
 ret 4
}}
extern "C" __declspec(naked) int __stdcall PlanningCaller(House*){__asm {
 mov ecx,[esp+4]
 call ControlPredicate
 test al,al
 jz original
 lea eax,local
 mov eax,[eax]
 ret 4
original:
 mov eax,[esp+4]
 mov eax,[eax]
 ret 4
}}
extern "C" __declspec(noinline) void Enhancement0(){MemoryBarrier();}
extern "C" __declspec(noinline) void Enhancement1(){InterlockedIncrement(&passedForeign);}
static bool controlCallback(void*raw,uint32_t tag){
 if(tag==0){auto&c=*static_cast<CONTEXT*>(raw);auto*args=reinterpret_cast<uint32_t*>(uintptr_t(c.Esp));
  if(c.Ecx!=uint32_t(uintptr_t(&local))||args[1]!=uint32_t(uintptr_t(&allowed))||args[2]||
    ((args[0]!=drawReturn||args[3]!=0)&&(args[0]!=pruneReturn||args[3]!=1)))return false;
  args[2]=1;return true;
 }
 if(tag!=4||!controlEnabled)return false;
 auto&c=*static_cast<CONTEXT*>(raw);if(c.Ecx!=uint32_t(uintptr_t(&local))&&c.Ecx!=uint32_t(uintptr_t(&other)))return false;
 const CONTEXT before=c;const uint32_t caller=*reinterpret_cast<const uint32_t*>(uintptr_t(c.Esp));
 c.Eip=assist::controlparams::destination(caller,uint32_t(uintptr_t(&ControlAllowed)),uint32_t(uintptr_t(&ControlStrict)),ownerReturn,planningReturn);
 CONTEXT expected=before;expected.Eip=c.Eip;assert(!memcmp(&c,&expected,sizeof c));return true;
}
static double measure(bool active){LARGE_INTEGER a{},b{},f{};assert(QueryPerformanceFrequency(&f)&&QueryPerformanceCounter(&a));for(unsigned n=0;n<10000;++n){assert(PermissionCaller(&other)==int(active));assert(OwnerCaller(&other)==other.tag);assert(PlanningCaller(&other)==other.tag);}assert(QueryPerformanceCounter(&b));return double(b.QuadPart-a.QuadPart)*1000000.0/double(f.QuadPart);}
static bool sameDebug(const CONTEXT&a,const CONTEXT&b){return a.Dr0==b.Dr0&&a.Dr1==b.Dr1&&a.Dr2==b.Dr2&&a.Dr3==b.Dr3&&a.Dr7==b.Dr7;}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);setbuf(stdout,nullptr);
 assert(OwnerCaller(&other)==other.tag);ownerReturn=predicateReturn;assert(PlanningCaller(&other)==other.tag);planningReturn=predicateReturn;assert(ownerReturn!=planningReturn);
 assert(DrawCaller(&local,&allowed)==0);drawReturn=returned;assert(PruneCaller(&local,&allowed)==2);pruneReturn=returned;
 const uint32_t sites[5]={uint32_t(uintptr_t(&SampleCanBuild)),uint32_t(uintptr_t(&foreignMarker)),uint32_t(uintptr_t(&Enhancement0)),uint32_t(uintptr_t(&Enhancement1)),uint32_t(uintptr_t(&ControlPredicate))};
 CONTEXT original=debug();assert(!original.Dr0&&!original.Dr1&&!original.Dr2&&!original.Dr3&&!(original.Dr7&0xffff00ffu));
 uint8_t code[9][96]{};const void*functions[]={reinterpret_cast<void*>(&ControlPredicate),reinterpret_cast<void*>(&ControlAllowed),reinterpret_cast<void*>(&ControlStrict),reinterpret_cast<void*>(&PermissionCaller),reinterpret_cast<void*>(&OwnerCaller),reinterpret_cast<void*>(&PlanningCaller),reinterpret_cast<void*>(&SampleCanBuild),reinterpret_cast<void*>(&DrawCaller),reinterpret_cast<void*>(&PruneCaller)};
 for(unsigned i=0;i<9;++i)memcpy(code[i],functions[i],96);const House savedLocal=local,savedOther=other;
 const double baselineUs=measure(false);assist::techbp::Service service;
 assert(service.inspect());wait(service);auto budget=service.slotBudget();assert(assist::slotbudget::known(budget)&&assist::slotbudget::free(budget)==4&&!assist::slotbudget::owned(budget)&&!service.handler_&&sameDebug(original,debug()));retire(service);
 // Read-only inspection must neither install a VEH nor change either enabled
 // foreign slot. Compare the actual Windows readback, including reserved bits.
 CONTEXT foreign=original;foreign.Dr0=sites[1];foreign.Dr1=sites[1];foreign.Dr7|=1|4;debug(&foreign);foreign=debug();assert(foreign.Dr0==sites[1]&&foreign.Dr1==sites[1]);assert(service.inspect());wait(service);budget=service.slotBudget();printf("BUDGET foreign2 packed=%08x state=%ld free=%u foreign=%u handler=%d\n",budget,service.state,assist::slotbudget::free(budget),assist::slotbudget::foreign(budget),int(service.handler_!=nullptr));assert(assist::slotbudget::foreign(budget)==2&&assist::slotbudget::free(budget)==2&&!service.handler_&&sameDebug(foreign,debug()));retire(service);debug(&original);
 assert(service.configure(sites,16,controlCallback));wait(service);assert(service.activeTags()==16&&__builtin_popcount(service.lease.mask)==1);
 assert(PermissionCaller(&local)==1&&PermissionCaller(&other)==1);assert(OwnerCaller(&local)==local.tag&&OwnerCaller(&other)==other.tag);assert(PlanningCaller(&local)==local.tag&&PlanningCaller(&other)==other.tag);
 controlEnabled=false;assert(PermissionCaller(&other)==0&&OwnerCaller(&other)==other.tag);controlEnabled=true;
 const LONG hits=service.exceptions,changed=service.changed;const double activeUs=measure(true);assert(service.exceptions-hits==30000&&service.changed-changed==30000);
 printf("CONTROL_QPC calls=30000 slots=1 baseline_us=%.3f active_us=%.3f additional_us_per_exception=%.6f\n",baselineUs,activeUs,(activeUs-baselineUs)/30000.0);
 // Joint Full + Control: two physical slots in ONE service/VEH.
 assert(service.configure(sites,17,controlCallback));wait(service);assert(service.activeTags()==17&&__builtin_popcount(service.lease.mask)==2);assert(DrawCaller(&local,&allowed)==1&&PruneCaller(&local,&allowed)==3&&PermissionCaller(&other)==1);
 assert(service.configure(sites,21,controlCallback));wait(service);assert(service.activeTags()==21&&__builtin_popcount(service.lease.mask)==3);budget=service.slotBudget();assert(assist::slotbudget::owned(budget)==3&&assist::slotbudget::free(budget)==1&&assist::slotbudget::tags(budget)==21&&assist::slotbudget::allows(budget,29));
 assert(service.configure(sites,29,controlCallback));wait(service);assert(service.activeTags()==29&&__builtin_popcount(service.lease.mask)==4);budget=service.slotBudget();assert(assist::slotbudget::owned(budget)==4&&!assist::slotbudget::free(budget)&&!assist::slotbudget::allows(budget,31));
 assert(service.configure(sites,21,controlCallback));wait(service);assert(service.activeTags()==21&&__builtin_popcount(service.lease.mask)==3);
 CONTEXT before=debug();assert(service.configure(sites,31,controlCallback));wait(service);assert(service.state==assist::techbp::Slots&&service.activeTags()==21&&sameDebug(before,debug()));
 // Removing control leaves the original Full sites alive, then use Enh+Control.
 assert(service.configure(sites,1,controlCallback));wait(service);assert(service.activeTags()==1&&PermissionCaller(&other)==0&&DrawCaller(&local,&allowed)==1);
 assert(service.configure(sites,28,controlCallback));wait(service);assert(service.activeTags()==28&&PermissionCaller(&other)==1);
 assert(service.configure(sites,15,controlCallback));wait(service);before=debug();assert(service.activeTags()==15);assert(service.configure(sites,31,controlCallback));wait(service);assert(service.state==assist::techbp::Slots&&service.activeTags()==15&&sameDebug(before,debug()));
 retire(service);assert(sameDebug(original,debug()));
 // A readback failure when a physical slot changes from tag 0 to tag 4 must
 // retain both possible owners and the handler until actual retirement.
 assert(service.configure(sites,1,controlCallback));wait(service);failReadAt=contextReads+2;assert(service.configure(sites,16,controlCallback));wait(service);assert(service.state==assist::techbp::System&&service.lease.mask&&service.lease.uncertain&&service.resources());
 assert(PermissionCaller(&other)==1);failReadAt=0;retire(service);assert(sameDebug(original,debug()));
 // Mixed foreign single-step causes pass the COMPLETE context unchanged.
 assert(service.configure(sites,16,controlCallback));wait(service);CONTEXT mixed=debug();mixed.Eip=sites[4];mixed.Dr6=service.lease.mask|(1u<<14);const CONTEXT untouched=mixed;
 EXCEPTION_RECORD record{};record.ExceptionCode=EXCEPTION_SINGLE_STEP;EXCEPTION_POINTERS ep{&record,&mixed};assert(assist::techbp::Service::vectored(&ep)==EXCEPTION_CONTINUE_SEARCH&&!memcmp(&mixed,&untouched,sizeof mixed));retire(service);
 for(unsigned i=0;i<9;++i)assert(!memcmp(code[i],functions[i],96));assert(!memcmp(&local,&savedLocal,sizeof local)&&!memcmp(&other,&savedOther,sizeof other));assert(sameDebug(original,debug()));
 puts("PASS control hardware: one real execution breakpoint; strict owner and planning callers; unchanged code and House data; read-only free/foreign/owned admission budgets; joint 2/3/4-slot transactions; 5-site refusal preserves existing groups; same-slot uncertain tag4 cleanup; mixed foreign exception context preserved; immediate off; complete retirement");
}
