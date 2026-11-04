// Real Windows own-process DR/VEH. Delay the production worker so an early
// return cannot be hidden by favorable scheduling. No game process is opened.
#define main legacy_transaction_fixture_not_called
#include "tech_ui_breakpoint_windows.cpp"
#undef main
static DWORD WINAPI wrongThreadAudit(void*raw){return static_cast<assist::techbp::Service*>(raw)->audit()?1:0;}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);setbuf(stdout,nullptr);
 auto foreign=AddVectoredExceptionHandler(0,foreignHandler);assert(foreign);
 CONTEXT original=debug();assert(!original.Dr0&&!original.Dr1&&!original.Dr2&&!original.Dr3);
 assert(DrawCaller(&local,&allowed)==0);const uint32_t a=returned-5;
 assert(PruneCaller(&local,&allowed)==2);const uint32_t b=returned-5;
 const uint32_t sites[assist::techbp::SiteCount]={a,b};
 assist::techbp::Service service;suspendDelayMs=15;
 DWORD started=GetTickCount();assert(service.enable(a,b,apply));
 assert(!service.pending()&&service.state==assist::techbp::Ready&&GetTickCount()-started>=15);
 assert(DrawCaller(&local,&allowed)==1&&PruneCaller(&local,&allowed)==3);
 for(unsigned n=0;n<32;++n){
  const unsigned wanted=(n&1)?3:1;
  assert(service.configure(sites,wanted,apply));assert(!service.pending()&&service.activeTags()==wanted);
  assert(DrawCaller(&local,&allowed)==1&&PruneCaller(&local,&allowed)==((wanted&2)?3:2));
  assert(service.audit());assert(!service.pending()&&service.state==assist::techbp::Ready);
 }
 // A live owner cannot be suspended by a transaction started on another
 // caller. The rejected request leaves its existing healthy lease intact.
 HANDLE other=CreateThread(nullptr,0,wrongThreadAudit,&service,0,nullptr);assert(other);
 assert(WaitForSingleObject(other,5000)==WAIT_OBJECT_0);DWORD result=1;assert(GetExitCodeThread(other,&result)&&result==0);CloseHandle(other);
 assert(!service.pending()&&service.activeTags()==3&&service.state==assist::techbp::Ready);
 const LONG before=passedForeign;RaiseException(EXCEPTION_SINGLE_STEP,0,0,nullptr);assert(passedForeign==before+1);
 suspendDelayMs=0;retire(service);CONTEXT after=debug();
 assert(after.Dr0==original.Dr0&&after.Dr1==original.Dr1&&after.Dr2==original.Dr2&&after.Dr3==original.Dr3&&after.Dr7==original.Dr7);
 assert(RemoveVectoredExceptionHandler(foreign));
 puts("PASS: delayed-worker owner rendezvous; 32 live reconfigurations and 32 audits; wrong-thread refusal preserves healthy lease; external single-step passes; exact DR retirement; no game");
}
