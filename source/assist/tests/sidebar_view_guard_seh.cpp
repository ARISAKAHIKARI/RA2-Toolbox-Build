// Standalone Windows process; no game process, addresses or executable loaded.
// The product SEH boundary executes; capture/native-action/failure callbacks
// are test models. The separate Runtime fixture executes the real failure body.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdio>
#ifdef NDEBUG
#error This verification must execute all assertions; compile with -UNDEBUG.
#endif
extern "C" int __cdecl GuardedSidebarViewAction(unsigned,unsigned);
static unsigned depth=0,beginCount=0,endCount=0,actionCount=0,failCount=0,filterCount=0,busy=0;
static bool reject=false;
static volatile unsigned* noAccess=nullptr;
static void* widePair[2]{};static bool geometryUnknown=false;static int status=0;
static DWORD seenCode=0;static unsigned seenBoundary=0;
extern "C" unsigned BeginAssistCapture(){SetLastError(0xB001);if(reject)return 0;++beginCount;return ++depth;}
extern "C" void EndAssistCapture(unsigned token){assert(token==depth&&depth);--depth;++endCount;SetLastError(0xB002);}
extern "C" int AssistExceptionFilter(void* raw,unsigned boundary){
 auto*p=static_cast<EXCEPTION_POINTERS*>(raw);assert(p&&p->ExceptionRecord);
 seenCode=p->ExceptionRecord->ExceptionCode;seenBoundary=boundary;++filterCount;
 assert(seenCode==EXCEPTION_ACCESS_VIOLATION&&p->ExceptionRecord->NumberParameters>=2);
 assert(p->ExceptionRecord->ExceptionInformation[0]==1&&p->ExceptionRecord->ExceptionInformation[1]==reinterpret_cast<ULONG_PTR>(noAccess));
 return EXCEPTION_EXECUTE_HANDLER;
}
extern "C" void AssistSidebarViewFailed(){busy=0;status=-3;++failCount;SetLastError(0xB003);}
extern "C" int __cdecl AssistSidebarViewAction(unsigned action,unsigned value){
 ++actionCount;SetLastError(0xB004);
 if(action==23){busy=1;status=1;geometryUnknown=true;*noAccess=value;assert(false);}
 if(action==20){assert(depth==1);assert(GuardedSidebarViewAction(23,7)==0);assert(depth==1&&busy==0);return 29;}
 return int(value+5);
}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 noAccess=static_cast<volatile unsigned*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS));assert(noAccess);
 for(auto&p:widePair){p=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(p);}
 SetLastError(0x1234);assert(GuardedSidebarViewAction(22,11)==16);
 assert(GetLastError()==0x1234&&depth==0&&beginCount==1&&endCount==1&&actionCount==1&&!failCount&&!filterCount);
 SetLastError(0x2345);assert(GuardedSidebarViewAction(23,1)==0);
 assert(GetLastError()==0x2345&&depth==0&&busy==0&&beginCount==2&&endCount==2&&actionCount==2&&failCount==1&&filterCount==1);
 assert(seenCode==EXCEPTION_ACCESS_VIOLATION&&seenBoundary==19);
 assert(status==-3&&geometryUnknown);
 for(auto p:widePair){MEMORY_BASIC_INFORMATION info{};assert(VirtualQuery(p,&info,sizeof info)&&info.State==MEM_COMMIT);}
 reject=true;SetLastError(0x3456);assert(GuardedSidebarViewAction(22,99)==0);
 assert(GetLastError()==0x3456&&depth==0&&beginCount==2&&endCount==2&&actionCount==2);
 reject=false;SetLastError(0x4567);assert(GuardedSidebarViewAction(20,0)==29);
 assert(GetLastError()==0x4567&&depth==0&&busy==0&&beginCount==4&&endCount==4&&actionCount==4&&failCount==2&&filterCount==2);
 assert(status==-3&&geometryUnknown);
 for(auto p:widePair){MEMORY_BASIC_INFORMATION info{};assert(VirtualQuery(p,&info,sizeof info)&&info.State==MEM_COMMIT);assert(VirtualFree(p,0,MEM_RELEASE));}
 assert(VirtualFree(const_cast<unsigned*>(noAccess),0,MEM_RELEASE));
 puts("PASS: real Windows x86 SEH AV dispatch, boundary19, Capture LIFO/finally, failure busy cleanup, nested capture and LastError preservation; modeled callbacks, no game");
 return 0;
}
