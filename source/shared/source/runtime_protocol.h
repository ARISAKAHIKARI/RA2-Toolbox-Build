#pragma once
#include <windows.h>
#include <stdint.h>
namespace runtime {
constexpr uint32_t Version=3,Magic=0x52543236,WakeTag=0x52545736;
constexpr uint32_t Tactical=1,Cursor=2;
enum Stage : LONG { Unseen=0,CallbackSeen=1,Connected=2,EntryRejected=-1,
 LegacyRuntime=-2,ControllerBusy=-3,OwnerOpenFailed=-4,OwnerNotAlive=-5,
 WatchdogPinFailed=-6,WatchdogTimerFailed=-7,MappingFailed=-8 };
struct Control {
 uint32_t magic,version,pid,owner;
 volatile LONG enabled,ready,wakePending;
 DWORD heartbeat;
 volatile LONG dispatches,reentrant;
 // Runtime is the sole writer. Diagnostics never grant entry or consume work.
 volatile LONG diagnosticSeq,callbacks;
 volatile DWORD callbackTick,callbackThread;
 volatile LONG stage;
 volatile DWORD error,runtimeBase,hookRva;
 volatile DWORD faultSerial,faultCode,faultAddress,faultOperation,faultAccess,faultBoundary;
 volatile DWORD faultThread,faultTick,faultCommand,faultPhase,faultSource,faultTarget;
};
static_assert(sizeof(Control)==120,"Runtime transport ABI");
inline void mapName(wchar_t*out,DWORD pid,DWORD owner){
 const wchar_t* p=L"Local\\RA2Runtime_v3_";int i=0;while(*p)out[i++]=*p++;
 DWORD ids[2]={pid,owner};for(DWORD n:ids){wchar_t b[16];int count=0;do{b[count++]=wchar_t(L'0'+n%10);n/=10;}while(n);while(count)out[i++]=b[--count];out[i++]=L'_';}out[i]=0;
}
}
