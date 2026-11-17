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
 const Fn stop=fn("Stop"),reads=fn("GameReadCount");
 assert(start()==1);
 // Never synchronously wait for a worker on the game's message callback.
 // This is an external test harness polling in a separate process.
 int result=0;
 for(int attempt=0;attempt<150&&!result;++attempt){Sleep(10);result=poll();}
 assert(result==1&&reads()==0);
 assert(stale()==1);
 bool stopped=false;
 for(int attempt=0;attempt<150;++attempt){
  if(stop()==1){stopped=true;break;}
  Sleep(10);
 }
 assert(stopped&&reads()==0);
 assert(FreeLibrary(dll));
 std::puts("PASS real Win32 command worker x86/x64: full graph off native owner thread, exact ticket and clean pin retirement");
}
