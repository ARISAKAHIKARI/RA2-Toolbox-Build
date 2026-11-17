#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdio>
using Fn=int(*)();
int main(int argc,char**argv){
 assert(argc>=2);
 HMODULE dll=LoadLibraryA(argv[1]);assert(dll);
 auto fn=[&](const char*name)->Fn{
  auto result=reinterpret_cast<Fn>(GetProcAddress(dll,name));assert(result);return result;
 };
 const Fn start=fn("Start"),poll=fn("Poll"),stale=fn("NeverAdmitStale");
 const Fn startGlobal=fn("StartGlobal"),pollGlobal=fn("PollGlobal");
 const Fn wrongGlobal=fn("PollWrongGlobal"),rejected=fn("RejectedCount");
 const Fn startExpired=fn("StartExpired"),workerReads=fn("WorkerReadCount");
 const Fn stop=fn("Stop"),reads=fn("GameReadCount");
 // Read a sealed global-vehicle roster but refuse to adopt it when the
 // live setting was switched off before the exact-frame handoff.
 const int rejectedBefore=rejected();
 assert(startGlobal()==1);
 bool wrongModeRejected=false;
 for(int attempt=0;attempt<150&&!wrongModeRejected;++attempt){
  Sleep(10);
  assert(wrongGlobal()==0);
  wrongModeRejected=rejected()>rejectedBefore;
 }
 assert(wrongModeRejected&&reads()==0);
 assert(start()==1);
 // Never synchronously wait for a worker on the game's message callback.
 // This is an external test harness polling in a separate process.
 int result=0;
 for(int attempt=0;attempt<150&&!result;++attempt){Sleep(10);result=poll();}
 assert(result==1&&reads()==0);
 // A new worker result with the matching setting still adopts correctly.
 assert(startGlobal()==1);
 result=0;
 for(int attempt=0;attempt<150&&!result;++attempt){Sleep(10);result=pollGlobal();}
 assert(result==1&&reads()==0);
 assert(stale()==1);
 // An expired same-frame request must not waste worker CPU on a graph.
 const int priorRejected=rejected(),priorWorkerReads=workerReads();
 assert(startExpired()==1);
 for(int attempt=0;attempt<150&&rejected()==priorRejected;++attempt)Sleep(10);
 assert(rejected()>priorRejected&&workerReads()==priorWorkerReads&&reads()==0);
 bool stopped=false;
 for(int attempt=0;attempt<150;++attempt){
  if(stop()==1){stopped=true;break;}
  Sleep(10);
 }
 assert(stopped&&reads()==0);
 assert(FreeLibrary(dll));
 std::puts("PASS real Win32 command worker x86/x64: off-thread capture, global-mode handoff, expired RPM rejection, and clean pin retirement");
}
