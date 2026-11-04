#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdio>
#ifdef NDEBUG
#error Exact TechLevel SEH verification requires active assertions.
#endif
extern "C" int __cdecl GuardedTechEnhancementProbe(unsigned,unsigned);
extern "C" int __cdecl GuardedTechUiParameter(void*,unsigned);
static unsigned depth=0,begins=0,ends=0,calls=0;static bool reject=false;static volatile unsigned*noAccess=nullptr;
extern "C" unsigned BeginTechEnhancementProbe(){SetLastError(0xB001);if(reject||depth>=16)return 0;++begins;return ++depth;}
extern "C" void EndTechEnhancementProbe(unsigned token){assert(token==depth&&depth);--depth;++ends;SetLastError(0xB002);}
extern "C" int __stdcall AssistTechEnhancementProbeCall(unsigned house,unsigned type){assert(house==7);++calls;SetLastError(0xB003);
 if(type==2){*noAccess=1;assert(false);}if(type==3){assert(depth==1);assert(GuardedTechEnhancementProbe(7,2)==0&&depth==1&&GetLastError()==0xB003);}return int(type+10);}
extern "C" int __cdecl AssistTechUiParameter(void*raw,unsigned tag){if(tag==2)*static_cast<volatile unsigned*>(raw)=42;return int(tag+1);}
int main(){SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);noAccess=static_cast<volatile unsigned*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS));assert(noAccess);
 SetLastError(0x1234);assert(GuardedTechEnhancementProbe(7,1)==11&&GetLastError()==0x1234&&!depth&&begins==1&&ends==1&&calls==1);
 SetLastError(0x2345);assert(GuardedTechEnhancementProbe(7,2)==0&&GetLastError()==0x2345&&!depth&&begins==2&&ends==2&&calls==2);
 SetLastError(0x3456);assert(GuardedTechEnhancementProbe(7,3)==13&&GetLastError()==0x3456&&!depth&&begins==4&&ends==4&&calls==4);
 reject=true;SetLastError(0x4567);assert(GuardedTechEnhancementProbe(7,1)==0&&GetLastError()==0x4567&&!depth&&begins==4&&ends==4&&calls==4);reject=false;
 for(unsigned i=1;i<=16;++i)assert(BeginTechEnhancementProbe()==i);SetLastError(0x5678);assert(GuardedTechEnhancementProbe(7,1)==0&&GetLastError()==0x5678&&calls==4);for(unsigned i=16;i;--i)EndTechEnhancementProbe(i);assert(!depth&&begins==ends);
 assert(GuardedTechUiParameter(nullptr,0)==1&&GuardedTechUiParameter(const_cast<unsigned*>(noAccess),2)==0);
 assert(VirtualFree(const_cast<unsigned*>(noAccess),0,MEM_RELEASE));puts("PASS: real Windows x86 product Probe SEH success/AV/nested/reject/depth16; finally LIFO and LastError; classifier AV fails closed; modeled admission/engine callbacks, no game");return 0;}
