#include "../../shared/source/runtime_fault.h"
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
extern "C" __declspec(dllimport) void __stdcall SetLastError(unsigned long);
extern "C" void* _exception_info();
extern "C" unsigned long _exception_code();
struct ExceptionRecord32 {unsigned code,flags;void* nested;void* address;unsigned count,info[15];};
struct ExceptionPointers32 {ExceptionRecord32* record;void* context;};
static_assert(sizeof(void*)==4&&sizeof(ExceptionRecord32)==80,"Windows x86 SEH ABI");
extern "C" int AssistExceptionFilter(void* raw,unsigned boundary){
 const auto* pointers=(const ExceptionPointers32*)raw;const auto* record=pointers?pointers->record:nullptr;
 if(record){
  const bool access=(record->code==0xc0000005u||record->code==0xc0000006u)&&record->count>=2;
  AssistReportException(boundary,record->code,(unsigned)record->address,access?record->info[0]:~0u,access?record->info[1]:0);
 }
 return 1;
}
// A failed native legality query rejects this batch before submission. SEH
// bounds faults, not side effects of platform/MOD overrides. UI marks this risk.
extern "C" int GuardedEnterAction(unsigned address,unsigned source,unsigned target,unsigned* action){
 __try {*action=((unsigned(__thiscall*)(void*,void*,bool))address)((void*)source,(void*)target,true);return 1;}
 __except(_exception_code()==0xc0000005u?AssistExceptionFilter(_exception_info(),9):0){return 0;}
}
// YRpp virtual SelectWeapon (+2E4), GetWeapon (+3F8), IsCloseEnough (+3A8).
// Explicit high-risk compatibility path. Platform/MOD virtual-method side
// effects are not verified; default callers use the raw query wrapper instead.
extern "C" int GuardedCombatWeapon(unsigned source,unsigned target,unsigned* weapon,int* selected,int* close,int* airborne){
 __try {
  auto table=*(unsigned**)source;
  // Only buildings have a meaningful powered-defense gate here.
  if(*(unsigned*)source==0x7E3EBCu&&!((bool(__thiscall*)(void*))table[0x350/4])((void*)source))return 0;
  int slot=((int(__thiscall*)(void*,void*))table[0x2E4/4])((void*)source,(void*)target);
  if(slot<0||slot>=18)return 0;
  auto info=((unsigned*(__thiscall*)(void*,int))table[0x3F8/4])((void*)source,slot);
  if(!info||!*info)return 0;
  *close=((bool(__thiscall*)(void*,void*,int))table[0x3A8/4])((void*)source,(void*)target,slot);
  *airborne=((bool(__thiscall*)(void*))(*(unsigned**)target)[0x54/4])((void*)target);
  *selected=slot;*weapon=*info;return 1;
 }__except(_exception_code()==0xc0000005u?AssistExceptionFilter(_exception_info(),9):0 ){return -1;}
}
extern "C" int GuardedObjectAir(unsigned object,int* airborne){
 __try {*airborne=((bool(__thiscall*)(void*))(*(unsigned**)object)[0x54/4])((void*)object);return 1;}
 __except(_exception_code()==0xc0000005u?AssistExceptionFilter(_exception_info(),9):0){return 0;}
}
extern "C" unsigned BeginAssistCapture();
extern "C" void EndAssistCapture(unsigned);
extern "C" int __cdecl AssistRiskAction(unsigned,unsigned,unsigned);
extern "C" void AssistFault();
extern "C" int __cdecl GuardedRiskAction(unsigned op,unsigned object,unsigned house){
 const unsigned long savedError=GetLastError();
 unsigned token=BeginAssistCapture();if(!token){SetLastError(savedError);return 0;}
 int result=0;
 __try {
  __try {result=AssistRiskAction(op,object,house);}
  __except(AssistExceptionFilter(_exception_info(),11)){AssistFault();}
 }__finally {EndAssistCapture(token);SetLastError(savedError);}
 return result;
}
// Compile this tiny boundary with the MSVC x86 ABI. MinGW x86's DWARF
// exception mode cannot emit Windows SEH funclets reliably.
extern "C" void AssistTick();
extern "C" void AssistFault();
extern "C" void GuardedAssistTick() {
    __try { AssistTick(); }
    __except(AssistExceptionFilter(_exception_info(),1)) { AssistFault(); }
}


extern "C" int AssistFilterMessage(void*);
extern "C" int GuardedAssistFilter(void* message){
 __try{return AssistFilterMessage(message);}
 __except(AssistExceptionFilter(_exception_info(),10)){AssistFault();return 0;}
}


extern "C" int GuardedViewResize(unsigned enableZoom,int width,int height){
 __try {((void(__cdecl*)())enableZoom)();return ((bool(__fastcall*)(int,int))0x560BF0)(width,height)?1:0;}
 __except(AssistExceptionFilter(_exception_info(),12)){return -1;}
}


// Native sender callback: the outer wrapper owns one capture lease across all
// original packing/timing/transport calls; cleanup runs before releasing it.
extern "C" int __cdecl AssistNetworkSend(unsigned,unsigned,int,int,int);
extern "C" void __cdecl AssistNetworkSenderAbort();
extern "C" int __cdecl GuardedNetworkSend(unsigned transport,unsigned buffer,int capacity,int delay,int serial){
 const unsigned long savedError=GetLastError();
 unsigned token=BeginAssistCapture();if(!token){SetLastError(savedError);return 0;}
 int result=0;
 __try {
  __try {result=AssistNetworkSend(transport,buffer,capacity,delay,serial);}
  __except(AssistExceptionFilter(_exception_info(),13)){AssistNetworkSenderAbort();}
 }__finally {EndAssistCapture(token);SetLastError(savedError);}
 return result;
}

// A failed local text query retains the original tooltip and other services.
extern "C" unsigned AssistBuildTimeTooltip(unsigned,unsigned);
extern "C" void AssistBuildTimeTooltipFailed();
extern "C" unsigned GuardedBuildTimeTooltip(unsigned id,unsigned original){
 const unsigned long savedError=GetLastError();
 unsigned token=BeginAssistCapture();if(!token){SetLastError(savedError);return original;}
 unsigned result=original;
 __try {
  __try {result=AssistBuildTimeTooltip(id,original);}
  __except(AssistExceptionFilter(_exception_info(),17)){AssistBuildTimeTooltipFailed();result=original;}
 }__finally {EndAssistCapture(token);SetLastError(savedError);}
 return result;
}

// CRC leaves return the already-loaded native EBX on failure. No game writes
// or CRC/RNG recomputation in this boundary; capture is owned by native logic.
extern "C" unsigned __cdecl AssistMapCrcShadow(unsigned,unsigned,unsigned);
extern "C" void __cdecl AssistMapCrcAbort();
extern "C" unsigned __cdecl GuardedMapCrcShadow(unsigned op,unsigned house,unsigned savedEBX){
 const unsigned long savedError=GetLastError();unsigned result=savedEBX;
 __try {result=AssistMapCrcShadow(op,house,savedEBX);}
 __except(AssistExceptionFilter(_exception_info(),14)){AssistMapCrcAbort();}
 SetLastError(savedError);return result;
}
