// Standalone Windows process; no game process or game executable loaded.
// 0x692300 belongs to our fixed-base image and fixture-only BSS arena.
// The runner fixes ONLY this test PE base/ASLR; the product guard is unmodified.
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
extern "C" int __cdecl GuardedNativeViewAction(unsigned,unsigned);
extern "C" int __cdecl GuardedNativeViewQuery(void*,void*,void*,void*,void*,void*);
extern "C" unsigned __cdecl GuardedNativeViewWndEnter(unsigned,unsigned);
extern "C" unsigned __cdecl GuardedNativeViewWndLeave();
extern "C" int __cdecl GuardedNativeViewMousePoint(void*);
static unsigned depth=0,beginCount=0,endCount=0,actionCount=0,failCount=0,filterCount=0,busy=0;
static bool reject=false,queryReject=false,unlockFault=false;
static unsigned queryDepth=0,queryBegin=0,queryEnd=0,queryCall=0,nativeFail=0,unlocks=0;
static volatile unsigned* noAccess=nullptr;
static void* widePair[2]{};static bool geometryUnknown=false;static int status=0;
static DWORD seenCode=0;static unsigned seenBoundary=0;
// The loader reserves this arena before choosing thread stacks/heap mappings.
// A fixed VirtualAlloc in main raced those reservations when the test had ASLR.
static volatile unsigned char queryArena[0x400000];
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
extern "C" unsigned BeginNativeViewQuery(){SetLastError(0xC001);if(queryReject||queryDepth>=16)return 0;++queryBegin;return ++queryDepth;}
extern "C" void EndNativeViewQuery(unsigned token){assert(token==queryDepth&&queryDepth);--queryDepth;++queryEnd;SetLastError(0xC002);}
extern "C" void __attribute__((thiscall)) FixtureNativeQuery(void*self,void*point,void*cell,void*coord,void*object,void*flagA,void*flagB){
 assert(self==reinterpret_cast<void*>(0x87F7E8)&&cell==reinterpret_cast<void*>(2)&&coord==reinterpret_cast<void*>(3)&&object==reinterpret_cast<void*>(4)&&flagA==reinterpret_cast<void*>(5)&&flagB==reinterpret_cast<void*>(6));
 ++queryCall;SetLastError(0xC003);const unsigned action=unsigned(reinterpret_cast<ULONG_PTR>(point));
 if(action==2){*noAccess=99;assert(false);}
 if(action==3){assert(queryDepth==1);assert(GuardedNativeViewQuery(reinterpret_cast<void*>(2),cell,coord,object,flagA,flagB)==0);assert(queryDepth==1);}
}
extern "C" int AssistNativeViewAction(unsigned action,unsigned argument){SetLastError(0xD001);if(action==1){busy=1;geometryUnknown=true;*noAccess=argument;assert(false);}return int(argument+9);}
extern "C" void AssistNativeViewFailed(){busy=0;status=-3;++nativeFail;SetLastError(0xD002);}
extern "C" void AssistNativeViewUnlockOne(){++unlocks;if(unlockFault){unlockFault=false;*noAccess=44;assert(false);}SetLastError(0xD003);}
extern "C" unsigned AssistNativeViewWndEnter(unsigned arg,unsigned ret){assert(ret==0x1234);SetLastError(0xE001);if(arg==2)*noAccess=5;return arg+1;}
extern "C" unsigned AssistNativeViewWndLeave(){SetLastError(0xE002);return 0x1234;}
extern "C" int AssistNativeViewMousePoint(void*p){SetLastError(0xE003);*static_cast<volatile unsigned*>(p)=9;return 1;}
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
 // Exact product call target, owned by our test image before process entry.
 // Never reserve an arbitrary free page or modify bytes outside queryArena.
 auto*code=reinterpret_cast<unsigned char*>(0x692300);
 assert(GetModuleHandleW(nullptr)==reinterpret_cast<HMODULE>(0x400000));
 assert(reinterpret_cast<ULONG_PTR>(code)>=reinterpret_cast<ULONG_PTR>(queryArena)&&reinterpret_cast<ULONG_PTR>(code)+5<=reinterpret_cast<ULONG_PTR>(queryArena)+sizeof queryArena);
 MEMORY_BASIC_INFORMATION owned{};assert(VirtualQuery(code,&owned,sizeof owned)&&owned.Type==MEM_IMAGE&&owned.State==MEM_COMMIT&&owned.AllocationBase==GetModuleHandleW(nullptr));
 DWORD oldQueryProtection=0;assert(VirtualProtect(code,5,PAGE_EXECUTE_READWRITE,&oldQueryProtection));
 code[0]=0xE9;*reinterpret_cast<unsigned*>(code+1)=unsigned(reinterpret_cast<ULONG_PTR>(&FixtureNativeQuery))-0x692305;
 assert(FlushInstructionCache(GetCurrentProcess(),code,5));
 auto query=[](unsigned action){return GuardedNativeViewQuery(reinterpret_cast<void*>(action),reinterpret_cast<void*>(2),reinterpret_cast<void*>(3),reinterpret_cast<void*>(4),reinterpret_cast<void*>(5),reinterpret_cast<void*>(6));};
 SetLastError(0x5678);assert(query(1)==1&&GetLastError()==0x5678&&queryDepth==0&&queryBegin==1&&queryEnd==1&&queryCall==1);
 SetLastError(0x6789);assert(query(2)==0&&GetLastError()==0x6789&&queryDepth==0&&queryBegin==2&&queryEnd==2&&queryCall==2&&seenBoundary==21);
 SetLastError(0x789A);assert(query(3)==1&&GetLastError()==0x789A&&queryDepth==0&&queryBegin==4&&queryEnd==4&&queryCall==4&&seenBoundary==21);
 queryReject=true;SetLastError(0x89AB);assert(query(1)==0&&GetLastError()==0x89AB&&queryDepth==0&&queryBegin==4&&queryCall==4);queryReject=false;
 SetLastError(0x9ABC);assert(GuardedNativeViewAction(0,4)==13&&GetLastError()==0x9ABC&&depth==0&&unlocks==2&&!nativeFail);
 SetLastError(0xABCD);assert(GuardedNativeViewAction(1,4)==0&&GetLastError()==0xABCD&&depth==0&&unlocks==4&&nativeFail==1&&busy==0&&status==-3&&seenBoundary==22);
 unlockFault=true;SetLastError(0xBCDE);assert(GuardedNativeViewAction(0,5)==14&&GetLastError()==0xBCDE&&depth==0&&unlocks==6&&nativeFail==2&&busy==0&&seenBoundary==22);
 SetLastError(0xCDEF);assert(GuardedNativeViewWndEnter(1,0x1234)==2&&GetLastError()==0xCDEF);assert(GuardedNativeViewWndEnter(2,0x1234)==~0u&&GetLastError()==0xCDEF);
 assert(GuardedNativeViewWndLeave()==0x1234&&GetLastError()==0xCDEF);unsigned output=0;assert(GuardedNativeViewMousePoint(&output)==1&&output==9&&GetLastError()==0xCDEF);
 assert(GuardedNativeViewMousePoint(const_cast<unsigned*>(noAccess))==0&&GetLastError()==0xCDEF);
 assert(VirtualProtect(code,5,oldQueryProtection,&owned.Protect));
 for(auto p:widePair){MEMORY_BASIC_INFORMATION info{};assert(VirtualQuery(p,&info,sizeof info)&&info.State==MEM_COMMIT);assert(VirtualFree(p,0,MEM_RELEASE));}
 assert(VirtualFree(const_cast<unsigned*>(noAccess),0,MEM_RELEASE));
 puts("PASS: Windows x86 SEH boundary19/21/22, Capture/query LIFO/finally, nested AV, busy cleanup, lock-finally AV, pure mouse/Wnd guards, LastError; modeled callbacks and owned query stub, no game");
 return 0;
}
