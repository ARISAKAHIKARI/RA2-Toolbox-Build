#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <initializer_list>
#include "frame_signal_policy.h"
namespace framesignal {
constexpr uint32_t Magic=0x46523234,Version=1,CommitSite=0x55DE81,Counter=0xA8ED84;
enum Consumer : LONG {Render=1,Compute=2,ExplicitDiagnostic=4};
enum State : LONG {Waiting=0,Native=1,MessageFallback=2,SignatureRejected=-1,RestoreFailed=-2};
struct alignas(8) Channel {
 uint32_t magic,version,pid,owner;
 volatile LONG enabled;volatile DWORD heartbeat;volatile LONG state,frame;
 volatile LONG seq,serial;volatile DWORD thread,error;
 volatile int64_t qpc,frequency;
};
static_assert(sizeof(Channel)==64&&offsetof(Channel,seq)==32&&offsetof(Channel,qpc)==48,"Frame signal x86/x64 ABI");
inline void name(wchar_t*out,DWORD pid,DWORD owner,bool event=false){
 const wchar_t*p=event?L"Local\\RA2FrameEvent_v1_":L"Local\\RA2Frame_v1_";int i=0;while(*p)out[i++]=*p++;
 for(DWORD n:{pid,owner}){wchar_t b[16];int count=0;do{b[count++]=wchar_t(L'0'+n%10);n/=10;}while(n);while(count)out[i++]=b[--count];out[i++]=L'_';}out[i]=0;
}
struct Stamp {LONG serial=0,frame=-1,state=Waiting;int64_t qpc=0,frequency=0;};
inline bool read(Channel*c,Stamp&out){
 if(!c)return false;for(int n=0;n<4;++n){LONG s=InterlockedCompareExchange(&c->seq,0,0);if(s&1)continue;
  Stamp v{c->serial,c->frame,c->state,c->qpc,c->frequency};MemoryBarrier();if(s==InterlockedCompareExchange(&c->seq,0,0)){out=v;return true;}}
 return false;
}
}
