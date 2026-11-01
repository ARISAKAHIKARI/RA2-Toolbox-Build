// Synthetic Win32 process, never a game process. The fixture runner fixes
// this test's image base at 0x400000; its dedicated arena owns all game VAs.
#define UNICODE
#define _UNICODE
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "../../cursor/source/hide.cpp"
#include "../../game_runtime/source/native_tooltip_runtime.h"
#include "../../game_runtime/source/frame_signal_runtime.h"
static DWORD testThread,tokenSerial,depth,tokens[16];
static gameentry::Client clients[16];
static bool tooltipOwned,frameOwned,failRestore,failInstallAfterWrite;
static unsigned patchCalls,providerCalls,appendCalls,readyCalls;
extern "C" const uint8_t testArena[0x710000];
__asm__(".section .text$fixture,\"xr\"\n.globl _testArena\n_testArena:\n.zero 0x710000\n.text\n");
static unsigned lastId;static void* lastReceiver;
static wchar_t unitName[]=L"unit",price[]=L"power plant\n$800",withTime[]=L"power plant\n$800\n20 seconds";
extern "C" void GuardedCursorResource(){}
extern "C" void RuntimeFrameReady(){++readyCalls;}
extern "C" unsigned GuardedBuildTimeTooltip(unsigned id,unsigned original){++appendCalls;return id==1000?unsigned(uintptr_t(withTime)):original;}
extern "C" wchar_t* __thiscall TestSidebar(void*self,unsigned id){
 ++providerCalls;lastReceiver=self;lastId=id;
 return id==0?unitName:id==1000?price:nullptr;
}
extern "C" int32_t __stdcall gameentry::ManageGameEntry(gameentry::Request*r){
 using namespace gameentry;
 if(GetCurrentThreadId()!=testThread)return int32_t(Result::WrongThread);
 if(r->operation==Operation::Enter||r->operation==Operation::Capture){
  if(r->operation==Operation::Enter&&depth)return int32_t(Result::Busy);
  assert(depth<16);clients[depth]=r->client;tokens[depth]=++tokenSerial;r->token=tokens[depth++];return int32_t(Result::Ok);
 }
 if(r->operation==Operation::Leave){assert(depth&&r->token==tokens[depth-1]);--depth;r->token=depth?tokens[depth-1]:0;return int32_t(Result::Ok);}
 if(r->operation==Operation::Inspect){r->active=r->address==nativetooltip::Slot?tooltipOwned:frameOwned;r->installed=unsigned(tooltipOwned)+unsigned(frameOwned);return int32_t(Result::Ok);}
 if(!depth||r->token!=tokens[depth-1]||r->client!=clients[depth-1])return int32_t(Result::WrongOwner);
 if(r->operation==Operation::Validate)return int32_t(Result::Ok);
 assert(r->operation==Operation::Patch&&r->client==Client::Assist);++patchCalls;
 bool* owned=nullptr;
 if(r->address==nativetooltip::Slot){assert(r->length==4);owned=&tooltipOwned;}
 else {assert(r->address==framesignal::CommitSite&&r->length==6);owned=&frameOwned;}
 auto* site=(void*)(uintptr_t)r->address;
 if(memcmp(site,r->before,r->length)||(*owned&&failRestore))return int32_t(Result::PatchDenied);
 memcpy(site,r->after,r->length);*owned=!*owned;FlushInstructionCache(GetCurrentProcess(),site,r->length);
 return int32_t(*owned&&failInstallAfterWrite?Result::SystemFailure:Result::Ok);
}
static void page(uint32_t address){
 // Never overwrite CRT/stack/heap reservations. The linker-owned arena
 // makes these absolute-address tests independent of allocation order.
 if(address<uintptr_t(testArena)||address+65536>uintptr_t(testArena)+sizeof testArena){
  fprintf(stderr,"Fixture arena %p..%p does not contain %08X\n",testArena,testArena+sizeof testArena,address);exit(3);
 }
 DWORD old=0;assert(VirtualProtect((void*)(uintptr_t)address,65536,PAGE_EXECUTE_READWRITE,&old));
}
static void jump(uint32_t address,void*fn){auto*p=(uint8_t*)(uintptr_t)address;p[0]=0xE9;uint32_t offset=uint32_t(uintptr_t(fn))-address-5;memcpy(p+1,&offset,4);}
static bool buildTime(bool on){gameentry::Scope scope(gameentry::Client::Assist);assert(scope);return NativeTooltipBuildTime(on);}
static wchar_t* tooltip(unsigned id){auto fn=*(uint32_t*)(uintptr_t)nativetooltip::Slot;return ((wchar_t*(__thiscall*)(void*,unsigned))fn)((void*)0x123456,id);}
static Shared cursorState;static DWORD mouseVtable[11],mouseObject[17],otherMouse[17];
static void cursor(bool enabled,LONG count=-1){s=enabled?&cursorState:nullptr;held=enabled?mouseObject:nullptr;cursorState.state=enabled?1:0;mouseObject[3]=DWORD(count);}
static unsigned beforeFrame[9],afterFrame[9];static DWORD frameStack;
static void __declspec(naked) frameReturn(){__asm {ret}}
static void* frameThunk=(void*)&frameobserver::committedAdapter;
static void __declspec(naked) exerciseFrame(){
 __asm {
  pushfd
  pushad
  mov frameStack,esp
  mov eax,0x11111111
  mov ecx,0x22222222
  mov edx,42
  mov ebx,0x33333333
  mov ebp,0x44444444
  mov esi,0x55555555
  mov edi,0x66666666
  push 0xED7
  popfd
  pushfd
  pushad
  mov esi,esp
  lea edi,beforeFrame
  mov ecx,9
  cld
  rep movsd
  popad
  popfd
  call dword ptr [frameThunk]
  pushfd
  pushad
  mov esi,esp
  lea edi,afterFrame
  mov ecx,9
  cld
  rep movsd
  popad
  popfd
  mov esp,frameStack
  popad
  popfd
  ret
 }
}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 testThread=GetCurrentThreadId();
 for(uint32_t address:{0x470000u,0x550000u,0x6D0000u,0x7B0000u,0x7F0000u,0x870000u,0x880000u,0xA80000u})page(address);
 memcpy((void*)nativetooltip::Original,nativetooltip::NativeBody,sizeof(nativetooltip::NativeBody));
 const uint8_t getter[]={0x8B,0x41,0x0C,0xC3};memcpy((void*)0x7B89F0,getter,4);
 jump(nativetooltip::Provider,(void*)&TestSidebar);
 *(uint32_t*)nativetooltip::Slot=nativetooltip::Original;
 mouseVtable[1]=0x7B8A00;mouseVtable[3]=0x7B9930;mouseVtable[4]=0x7B9750;mouseVtable[10]=0x7B89F0;
 mouseObject[0]=uint32_t(uintptr_t(mouseVtable));memcpy(otherMouse,mouseObject,sizeof(mouseObject));
 *(void**)0x887640=mouseObject;FlushInstructionCache(GetCurrentProcess(),nullptr,0);
 // Reproduce the native regression with the actual 38-byte visibility wrapper.
 cursor(true);assert(!tooltip(0)&&!tooltip(1000)&&providerCalls==0);
 NativeTooltipMaintain(true);assert(tooltipOwned&&!NativeTooltipDetached());
 assert(tooltip(0)==unitName&&tooltip(1000)==price&&appendCalls==0);
 assert(providerCalls==2&&lastId==1000&&lastReceiver==(void*)nativetooltip::Sidebar&&LONG(mouseObject[3])==-1);
 // Alt-Tab releases only Windows presentation. The native hide and shared
 // tooltip slot remain owned; no ref-count or site writes on focus changes.
 unsigned focusPatches=patchCalls;cursorState.menuSuspend=2;nativeRefresh();NativeTooltipMaintain(true);
 assert(held==mouseObject&&LONG(mouseObject[3])==-1&&tooltipOwned&&patchCalls==focusPatches&&cursorState.nativeVisible==0);
 CWPRETSTRUCT focusEvent{};focusEvent.message=WM_SETCURSOR;
 CursorRuntimePost(0,0,(LPARAM)&focusEvent);
 assert(held==mouseObject&&patchCalls==focusPatches&&cursorState.menuSuspend==2);
 cursorState.menuSuspend=0;NativeTooltipMaintain(true);assert(tooltip(0)==unitName&&patchCalls==focusPatches);
 // Additional engine hide, no text, replaced object, foreign getter, and
 // unavailable capture all keep native null. No forced hidden-name flag.
 cursor(true,-2);assert(!tooltip(0));cursor(true);assert(!tooltip(99));
 *(void**)0x887640=otherMouse;otherMouse[3]=DWORD(-1);assert(!tooltip(0));*(void**)0x887640=mouseObject;
 mouseVtable[10]=0x7B89F1;assert(!CursorTooltipOwnsHiddenMouse());mouseVtable[10]=0x7B89F0;
 DWORD bound=testThread;testThread=0;assert(!tooltip(0));testThread=bound;
 // RefCount is never rewritten and helper-only work preserves LastError/FP.
 SetLastError(0x12345678);assert(tooltip(0)==unitName&&GetLastError()==0x12345678);
 unsigned mxcsr=0x5F80,afterMxcsr=0;__asm {ldmxcsr mxcsr}
 assert(tooltip(1000)==price);__asm {stmxcsr afterMxcsr}assert(afterMxcsr==mxcsr);mxcsr=0x1F80;__asm {ldmxcsr mxcsr}
 // Both features share exactly one slot. Turning the suffix off leaves the
 // unit/price fix running; turning cursor off leaves the suffix running.
 assert(buildTime(true)&&tooltip(1000)==withTime&&appendCalls==1);
 assert(buildTime(false)&&tooltipOwned&&tooltip(1000)==price);
 assert(buildTime(true));cursor(false,0);assert(tooltip(1000)==withTime);
 assert(buildTime(false)&&!tooltipOwned&&NativeTooltipDetached());
 // Resource-only/inactive cursor must not install a game target slot.
 s=&cursorState;cursorState.state=0;held=nullptr;unsigned patches=patchCalls;
 NativeTooltipMaintain(true);assert(patchCalls==patches&&!tooltipOwned);s=nullptr;
 // Unknown native body refuses installation instead of overwriting a mod.
 cursor(true);*(uint8_t*)nativetooltip::Original=0x90;NativeTooltipMaintain(true);assert(!tooltipOwned);
 *(uint8_t*)nativetooltip::Original=nativetooltip::NativeBody[0];
 failInstallAfterWrite=true;NativeTooltipMaintain(true);assert(tooltipOwned&&!NativeTooltipDetached());failInstallAfterWrite=false;
 cursor(false,0);failRestore=true;NativeTooltipMaintain(false);assert(tooltipOwned&&!NativeTooltipDetached());
 failRestore=false;NativeTooltipMaintain(false);assert(!tooltipOwned&&NativeTooltipDetached());
 // Foreign slot change remains untouched and keeps Runtime ownership pinned.
 cursor(true);NativeTooltipMaintain(true);auto ours=*(uint32_t*)nativetooltip::Slot;*(uint32_t*)nativetooltip::Slot=0x12345678;
 cursor(false,0);NativeTooltipMaintain(false);assert(!NativeTooltipDetached()&&*(uint32_t*)nativetooltip::Slot==0x12345678);
 *(uint32_t*)nativetooltip::Slot=ours;NativeTooltipMaintain(false);assert(NativeTooltipDetached());
 puts("PASS native tooltip: original suppression reproduced; names/prices, shared owners, native extra hides, ABI and cleanup");

 // Run the real compiled frame adapter with live registers, EFLAGS (DF=1),
 // native store, and a balanced return continuation. No game simulation.
 frameobserver::continuation=uint32_t(uintptr_t(&frameReturn));
 framesignal::Channel c{};c.enabled=framesignal::Compute;c.thread=testThread;c.heartbeat=GetTickCount();
 for(unsigned scenario=0;scenario<4;++scenario){
  frameobserver::channel=scenario?&c:nullptr;c.thread=scenario==2?0:testThread;c.heartbeat=scenario==3?GetTickCount()-1000:GetTickCount();
  alignas(16) uint8_t fpBefore[512]{},fpAfter[512]{};
  unsigned beforeReady=readyCalls;SetLastError(0x87654321);
  __asm {fld1}
  __asm {fxsave fpBefore}
  exerciseFrame();
  __asm {fxsave fpAfter}
  __asm {fstp st(0)}
  assert(!memcmp(fpBefore,fpAfter,sizeof fpBefore));
  assert(!memcmp(beforeFrame,afterFrame,sizeof beforeFrame)&&*(int*)framesignal::Counter==42&&GetLastError()==0x87654321);
  assert(readyCalls==beforeReady+(scenario==1?1:0));
 }
 // Exercise production maintain(): ordinary SP/LAN/Internet requests use
 // the native site without requiring the diagnostic-only consumer bit.
 memcpy((void*)framesignal::CommitSite,frameobserver::original,6);
 *(uint32_t*)0xA83D4C=0x123456;*(uint8_t*)0xA8E9A0=1;*(uint8_t*)0xA8B8C2=0;
 for(int mode:{0,5,3,4}){
  *(int*)0xA8B238=mode;c.thread=testThread;c.heartbeat=GetTickCount();c.state=framesignal::Waiting;c.enabled=framesignal::Compute;
  frameobserver::channel=&c;frameobserver::owner=GetCurrentProcessId();patches=patchCalls;
  frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&c.state==framesignal::Native&&patchCalls==patches+1);
  patches=patchCalls;c.enabled|=framesignal::ExplicitDiagnostic;frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&patchCalls==patches);
  *(uint8_t*)0xA8E9A0=0;c.heartbeat=GetTickCount()-1000;
  frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&c.state==framesignal::Native&&patchCalls==patches);
  unsigned readyBeforePause=readyCalls;frameobserver::FrameSignalCommitted();assert(readyCalls==readyBeforePause);
  *(uint8_t*)0xA8E9A0=1;c.heartbeat=GetTickCount();c.enabled=framesignal::Compute;
  frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&c.state==framesignal::Native&&patchCalls==patches);
  // The fixture's channel is stack-owned; remove that pointer before the
  // real owner-exit cleanup path, which still restores the managed site.
  frameobserver::channel=nullptr;frameobserver::maintain(0);assert(!frameOwned&&!frameobserver::installed);
 }
 for(int mode:{-1,0,1,2,3,4,5,6}){
  bool supported=mode==0||mode==5||mode==3||mode==4;
  assert(framesignal::nativeAllowed(mode,false)==supported&&framesignal::nativeAllowed(mode,true)==supported);
 }
 // Multiplayer still requires local playable state before NEW installation.
 *(int*)0xA8B238=3;c.state=framesignal::Waiting;c.enabled=framesignal::Compute;c.heartbeat=GetTickCount();
 frameobserver::channel=&c;frameobserver::owner=GetCurrentProcessId();patches=patchCalls;
 *(uint8_t*)0xA8E9A0=0;frameobserver::maintain(GetCurrentProcessId());assert(!frameOwned&&patchCalls==patches);
 *(uint8_t*)0xA8E9A0=1;*(uint8_t*)0xA8B8C2=1;frameobserver::maintain(GetCurrentProcessId());assert(!frameOwned&&patchCalls==patches);
 *(uint8_t*)0xA8B8C2=0;c.enabled=0;frameobserver::maintain(GetCurrentProcessId());assert(!frameOwned&&patchCalls==patches);
 frameobserver::channel=nullptr;assert(!depth);
 puts("PASS frame adapter: GPR/ESP/EFLAGS/FXSAVE preserved; native store; live/stale/absent callbacks; ordinary sessions 0/5/3/4 install; readiness and owner-exit cleanup");
 puts("PASS focus lifecycle: cursor hide/tooltip ownership retained; live-owner frame hook survives pause/stale sample and diagnostic-bit removal");
 puts("Synthetic Win32 execution only: no RA2OL/game visual or multiplayer claim.");
 return 0;
}
