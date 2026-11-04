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
static bool tooltipOwned,layoutOwned,frameOwned,failRestore,failInstallAfterWrite,failRestoreAfterWrite;
static unsigned patchCalls,providerCalls,appendCalls,readyCalls,cursorRestoreCalls;
static bool loseMouseReadAfterHide;
extern "C" const uint8_t testArena[0x710000];
__asm__(".section .text$fixture,\"xr\"\n.globl _testArena\n_testArena:\n.zero 0x710000\n.text\n");
static unsigned lastId;static void* lastReceiver;
static LONG CALLBACK fixtureException(PEXCEPTION_POINTERS p){
 if(p->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION)
  fprintf(stderr,"Fixture access violation: instruction=%p address=%08lX\n",p->ExceptionRecord->ExceptionAddress,DWORD(p->ExceptionRecord->ExceptionInformation[1]));
 return EXCEPTION_CONTINUE_SEARCH;
}
static wchar_t unitName[]=L"unit",price[]=L"power plant\n$800",withTime[]=L"power plant\n$800\n20 seconds";
extern "C" void GuardedCursorResource(){}
extern "C" void RuntimeFrameReady(){++readyCalls;}
extern "C" unsigned GuardedBuildTimeTooltip(unsigned id,unsigned original){++appendCalls;return id==1000?unsigned(uintptr_t(withTime)):original;}
extern "C" void __thiscall TestCursorRestore(void*self){++cursorRestoreCalls;++reinterpret_cast<LONG*>(self)[3];}
extern "C" void __thiscall TestCursorHide(void*self){
 --reinterpret_cast<LONG*>(self)[3];
 if(loseMouseReadAfterHide){DWORD old=0;assert(VirtualProtect(self,4096,PAGE_NOACCESS,&old));}
}
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
 if(r->operation==Operation::Inspect){r->active=r->address==nativetooltip::Slot?tooltipOwned:r->address==nativetooltip::LayoutSlot?layoutOwned:frameOwned;r->installed=unsigned(tooltipOwned)+unsigned(layoutOwned)+unsigned(frameOwned);return int32_t(Result::Ok);}
 if(!depth||r->token!=tokens[depth-1]||r->client!=clients[depth-1])return int32_t(Result::WrongOwner);
 if(r->operation==Operation::Validate)return int32_t(Result::Ok);
 assert(r->operation==Operation::Patch&&r->client==Client::Assist);++patchCalls;
 bool* owned=nullptr;
 if(r->address==nativetooltip::Slot){assert(r->length==4);owned=&tooltipOwned;}
 else if(r->address==nativetooltip::LayoutSlot){assert(r->length==4);owned=&layoutOwned;}
 else {assert(r->address==framesignal::CommitSite&&r->length==6);owned=&frameOwned;}
 auto* site=(void*)(uintptr_t)r->address;
 if(memcmp(site,r->before,r->length)||(*owned&&failRestore))return int32_t(Result::PatchDenied);
 memcpy(site,r->after,r->length);*owned=!*owned;FlushInstructionCache(GetCurrentProcess(),site,r->length);
 return int32_t((*owned&&failInstallAfterWrite)||(!*owned&&failRestoreAfterWrite)?Result::SystemFailure:Result::Ok);
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
static void cursor(bool enabled,LONG count=-1){s=enabled?&cursorState:nullptr;held=enabled?mouseObject:nullptr;heldVtable=enabled?mouseObject[0]:0;cursorState.state=enabled?1:0;mouseObject[3]=DWORD(count);}
// The optional private PE is read only into this fixture's own fixed-address
// arena. No game is opened, launched, written, or included in result artifacts.
static void nativeBytes(const char*path,uint32_t address,unsigned size){
 FILE*f=fopen(path,"rb");assert(f);IMAGE_DOS_HEADER dos{};assert(fread(&dos,1,sizeof dos,f)==sizeof dos&&dos.e_magic==IMAGE_DOS_SIGNATURE);
 assert(!fseek(f,dos.e_lfanew,SEEK_SET));IMAGE_NT_HEADERS32 nt{};assert(fread(&nt,1,sizeof nt,f)==sizeof nt&&nt.Signature==IMAGE_NT_SIGNATURE&&nt.OptionalHeader.ImageBase==0x400000);
 const uint32_t rva=address-nt.OptionalHeader.ImageBase;bool loaded=false;
 for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){IMAGE_SECTION_HEADER sec{};assert(fread(&sec,1,sizeof sec,f)==sizeof sec);
  if(rva>=sec.VirtualAddress&&uint64_t(rva)+size<=uint64_t(sec.VirtualAddress)+sec.SizeOfRawData){assert(!fseek(f,sec.PointerToRawData+rva-sec.VirtualAddress,SEEK_SET));assert(fread((void*)(uintptr_t)address,1,size,f)==size);loaded=true;break;}}
 fclose(f);assert(loaded);
}
extern "C" void __thiscall TestNativeTextMeasure(void*,const wchar_t*text,int*w,int*h,int width){assert(text&&width>0);*w=100;*h=20;}
extern "C" size_t __cdecl TestNativeTextLength(const wchar_t*s){return wcslen(s);}
extern "C" wchar_t* __cdecl TestNativeTextCopy(wchar_t*dest,const wchar_t*s,size_t n){return wcsncpy(dest,s,n);}
static DWORD tipObject[200],tipItem[7];
static void prepareTip(unsigned id,int x,int y){memset(tipObject,0,sizeof tipObject);memset(tipItem,0,sizeof tipItem);tipItem[0]=id;tipObject[0]=0x7F74C4;tipObject[1]=DWORD(uintptr_t(tipItem));tipObject[4]=x;tipObject[5]=y;}
static DWORD processStack,processBefore[4],processAfter[4],processResult;
static void __declspec(naked) checkedProcess(){__asm {
 pushfd
 pushad
 mov processStack,esp
 mov ebx,0x11223344
 mov ebp,0x22334455
 mov esi,0x33445566
 mov edi,0x44556677
 mov processBefore[0],ebx
 mov processBefore[4],ebp
 mov processBefore[8],esi
 mov processBefore[12],edi
 lea ecx,tipObject
 mov eax,0x724AD0
 call eax
 mov processResult,eax
 mov processAfter[0],ebx
 mov processAfter[4],ebp
 mov processAfter[8],esi
 mov processAfter[12],edi
 cmp esp,processStack
 je balanced
 int 3
 balanced:
 popad
 popfd
 ret
}}
static bool processTip(){checkedProcess();assert(!memcmp(processBefore,processAfter,sizeof processBefore));return (processResult&255)!=0;}
static void nativeTooltipPipeline(const char*path){
 nativeBytes(path,nativetooltip::LayoutOriginal,0x20e);nativeBytes(path,0x724AD0,0xad);
 jump(0x433CF0,(void*)&TestNativeTextMeasure);jump(0x7CA405,(void*)&TestNativeTextLength);jump(0x7CA422,(void*)&TestNativeTextCopy);
 *(DWORD*)0x89C4D0=0x123400;FlushInstructionCache(GetCurrentProcess(),nullptr,0);
 assert(nativetooltip::layoutCompatible());unsigned cases=0;
 for(int right:{0,1})for(unsigned id:{0u,1000u})for(int edge:{0,1}){
  *(BYTE*)0xA8EB7C=BYTE(right);for(uint32_t a:{0x886F90u,0x886FA0u}){auto*r=(int*)(uintptr_t)a;r[0]=0;r[1]=0;r[2]=640;r[3]=480;}
  const int x=edge?630:100,y=edge?470:100;
  cursor(false,0);NativeTooltipMaintain(false);prepareTip(id,x,y);assert(processTip());DWORD expected[200];memcpy(expected,tipObject,sizeof expected);
  cursor(true);prepareTip(id,x,y);assert(!processTip()&&!tipObject[1]); // unpatched Hide suppresses the text
  NativeTooltipMaintain(true);assert(tooltipOwned&&layoutOwned&&cursorState.tooltipState==1);
  // Reproduce the former incomplete fix: text succeeds, native layout still
  // returns false and clears the active item/rectangle.
  const DWORD adapter=*(DWORD*)nativetooltip::LayoutSlot;*(DWORD*)nativetooltip::LayoutSlot=nativetooltip::LayoutOriginal;
  prepareTip(id,x,y);assert(!processTip()&&!tipObject[1]);
  *(DWORD*)nativetooltip::LayoutSlot=adapter;
  prepareTip(id,x,y);assert(processTip()&&!memcmp(expected,tipObject,sizeof expected)&&LONG(mouseObject[3])==-1);
  cursor(true,-2);prepareTip(id,x,y);assert(!processTip()&&!tipObject[1]&&LONG(mouseObject[3])==-2);
  cursor(false,0);NativeTooltipMaintain(false);assert(!tooltipOwned&&!layoutOwned);++cases;
 }
 // Missing native text stays missing; a foreign layout cannot be overwritten
 // or falsely acknowledged as ready; retry preserves the independent text slot.
 cursor(true);NativeTooltipMaintain(true);prepareTip(99,100,100);assert(!processTip());
 DWORD ours=*(DWORD*)nativetooltip::LayoutSlot;*(DWORD*)nativetooltip::LayoutSlot=0x12345678;
 NativeTooltipMaintain(true);assert(cursorState.tooltipState==-1&&*(DWORD*)nativetooltip::LayoutSlot==0x12345678);
 cursor(false,0);NativeTooltipMaintain(false);assert(layoutOwned&&!tooltipOwned&&!NativeTooltipDetached());
 *(DWORD*)nativetooltip::LayoutSlot=ours;NativeTooltipMaintain(false);assert(NativeTooltipDetached());
 printf("PASS native complete tooltip pipeline: %u visible/hidden/text-only/fixed/extra-hide pairs; private native manager+layout; exact text+rectangle+active-item parity; callee registers/ESP; foreign-slot retry\n",cases);
 providerCalls=appendCalls=patchCalls=0;
}
static void cursorReadRecovery(){
 auto* object=reinterpret_cast<DWORD*>(0x890000);auto* table=reinterpret_cast<DWORD*>(0x8A0000);
 memcpy(table,mouseVtable,sizeof mouseVtable);memcpy(object,mouseObject,sizeof mouseObject);object[0]=DWORD(uintptr_t(table));
 // Real Runtime disconnect unmaps its view. Keep a second fixture view to
 // inspect the final ACK; a static Shared would not be a valid mapping.
 HANDLE testMapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),nullptr);assert(testMapping);
 auto* observed=(Shared*)MapViewOfFile(testMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));assert(observed);Shared&state=*observed;
 auto bind=[&]{
  if(s)UnmapViewOfFile(s);if(mapping)CloseHandle(mapping);
  assert(DuplicateHandle(GetCurrentProcess(),testMapping,GetCurrentProcess(),&mapping,0,FALSE,DUPLICATE_SAME_ACCESS));
  s=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));assert(s);memset(s,0,sizeof *s);
  held=object;heldVtable=object[0];state.state=1;state.heartbeat=GetTickCount();state.owner=GetCurrentProcessId();
  object[3]=DWORD(-1);hiddenRef=-1;*(void**)0x887640=object;
 };
 // Exercise checked reads at each identity level in the real implementation.
 // A transient global/object/vtable read failure is not object retirement.
 for(uint32_t address:{0x887000u,0x890000u,0x8A0000u}){
  bind();unsigned calls=cursorRestoreCalls;DWORD protection=0;
  assert(VirtualProtect((void*)address,4096,PAGE_NOACCESS,&protection));
  {gameentry::Scope scope(gameentry::Client::Cursor);assert(scope);inspect(false);}
  assert(held==object&&state.state==1&&state.command==0&&cursorRestoreCalls==calls);
  state.command=2;
  {gameentry::Scope scope(gameentry::Client::Cursor);assert(scope);tick();}
  assert(held==object&&state.state==1&&state.command==2&&state.error==4&&!CursorRuntimeDetached());
  CursorRuntimeWatchdog();
  assert(held==object&&state.command==2&&!CursorRuntimeDetached()&&cursorRestoreCalls==calls);
  DWORD unused=0;assert(VirtualProtect((void*)address,4096,protection,&unused));
  CursorRuntimeWatchdog();
  assert(!held&&CursorRuntimeDetached()&&state.state==0&&state.command==0&&state.error==0);
  assert(cursorRestoreCalls==calls+1&&LONG(object[3])==0);
 }
 // A verified new object, cleared singleton, or changed identity retires
 // our responsibility without calling Show on a stale or foreign object.
 for(unsigned replacement=0;replacement<4;++replacement){
  bind();unsigned calls=cursorRestoreCalls;const DWORD originalTable=object[0],originalSlot=table[1];
  if(replacement==0)*(void**)0x887640=otherMouse;
  else if(replacement==1)*(void**)0x887640=nullptr;
  else if(replacement==2)object[0]=DWORD(uintptr_t(mouseVtable));
  else table[1]=0x12345678;
  {gameentry::Scope scope(gameentry::Client::Cursor);assert(scope);inspect(false);}
  assert(!held&&state.state==-1&&cursorRestoreCalls==calls&&LONG(object[3])==-1);
  object[0]=originalTable;table[1]=originalSlot;
 }
 // Hide can commit before its confirmation read becomes unavailable. It
 // must remain pending rather than publishing a failure/detached ACK.
 bind();held=nullptr;heldVtable=0;state.state=0;object[3]=0;
 state.command=1;loseMouseReadAfterHide=true;unsigned calls=cursorRestoreCalls;
 {gameentry::Scope scope(gameentry::Client::Cursor);assert(scope);tick();}
 assert(held==object&&state.state==2&&state.command==2&&state.error==4&&cursorRestoreCalls==calls);
 DWORD unused=0;assert(VirtualProtect(object,4096,PAGE_EXECUTE_READWRITE,&unused));loseMouseReadAfterHide=false;
 CursorRuntimeWatchdog();
 assert(!held&&CursorRuntimeDetached()&&state.state==0&&state.command==0&&LONG(object[3])==0&&cursorRestoreCalls==calls+1);
 // Reusing an old mapping for a resource-only session still owes the prior
 // Hide, even when the new host overwrote its shared state/command fields.
 bind();state.command=3;calls=cursorRestoreCalls;
 assert(VirtualProtect(object,4096,PAGE_NOACCESS,&unused));
 {gameentry::Scope scope(gameentry::Client::Cursor);assert(scope);tick();}
 assert(held==object&&state.state==2&&state.command==2&&state.error==4&&cursorRestoreCalls==calls);
 assert(VirtualProtect(object,4096,PAGE_EXECUTE_READWRITE,&unused));CursorRuntimeWatchdog();
 assert(!held&&CursorRuntimeDetached()&&state.state==0&&state.command==0&&LONG(object[3])==0&&cursorRestoreCalls==calls+1);
 // Actual Cursor entry alone (no Assist client or native frame signal) can
 // enable Hide mid-session and acknowledge both tooltip slots, then detach.
 bind();held=nullptr;heldVtable=0;state.state=0;state.command=1;object[3]=0;
 CursorRuntimeMessage(0,PM_REMOVE,0);NativeTooltipMaintain(true);
 assert(state.state==1&&held==object&&LONG(object[3])==-1&&state.tooltipState==1&&tooltipOwned&&layoutOwned&&!frameOwned);
 state.command=2;CursorRuntimeMessage(0,PM_REMOVE,0);NativeTooltipMaintain(false);
 assert(CursorRuntimeDetached()&&!held&&LONG(object[3])==0&&NativeTooltipDetached());
 UnmapViewOfFile(observed);CloseHandle(testMapping);
 *(void**)0x887640=mouseObject;
 puts("PASS cursor restore: transient global/object/vtable reads retain ownership and retry; replacement avoids stale calls; post-Hide failure and resource-only restart retain pending restore");
 puts("PASS Cursor-only cold enable: actual message entry hides and owns both tooltip slots without Assist/frame features; stop restores");
}
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
int main(int argc,char**argv){
 setvbuf(stdout,nullptr,_IONBF,0);
 AddVectoredExceptionHandler(0,fixtureException);
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 testThread=GetCurrentThreadId();
 for(uint32_t address:{0x433000u,0x470000u,0x550000u,0x6D0000u,0x720000u,0x7B0000u,0x7C0000u,0x7F0000u,0x870000u,0x880000u,0x890000u,0x8A0000u,0xA80000u})page(address);
 memcpy((void*)nativetooltip::Original,nativetooltip::NativeBody,sizeof(nativetooltip::NativeBody));
 const uint8_t getter[]={0x8B,0x41,0x0C,0xC3};memcpy((void*)0x7B89F0,getter,4);
 jump(nativetooltip::Provider,(void*)&TestSidebar);
 jump(0x7B9750,(void*)&TestCursorRestore);
 jump(0x7B9930,(void*)&TestCursorHide);
 *(uint32_t*)nativetooltip::Slot=nativetooltip::Original;
 *(uint32_t*)nativetooltip::LayoutSlot=nativetooltip::LayoutOriginal;
 memcpy((void*)nativetooltip::LayoutOriginal,nativetooltip::LayoutPrefix,sizeof nativetooltip::LayoutPrefix);
 const uint8_t layoutTail[]={0x32,0xC0,0x83,0xC4,0x1C,0xC2,4,0};memcpy((void*)0x478DA6,layoutTail,sizeof layoutTail);
 mouseVtable[1]=0x7B8A00;mouseVtable[3]=0x7B9930;mouseVtable[4]=0x7B9750;mouseVtable[10]=0x7B89F0;
 mouseObject[0]=uint32_t(uintptr_t(mouseVtable));memcpy(otherMouse,mouseObject,sizeof(mouseObject));
 *(void**)0x887640=mouseObject;FlushInstructionCache(GetCurrentProcess(),nullptr,0);
 if(argc>1)nativeTooltipPipeline(argv[1]);
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
 // A paused controller can keep its process alive without a fresh heartbeat.
 // Retain hide and tooltip ownership, stop sampling, and honor explicit stop.
 owner=OpenProcess(SYNCHRONIZE,FALSE,GetCurrentProcessId());assert(owner);ownerId=cursorState.owner=GetCurrentProcessId();
 cursorState.heartbeat=GetTickCount()-6000;cursorState.command=0;lastSampleAt=0;
 const LONG staleSamples=cursorState.sampleAttempts;
 {gameentry::Scope scope(gameentry::Client::Cursor);assert(scope);tick();}
 CursorRuntimeWatchdog();NativeTooltipMaintain(true);
 assert(held==mouseObject&&cursorState.state==1&&cursorState.sampleAttempts==staleSamples&&patchCalls==focusPatches&&cursorRestoreCalls==0);
 CloseHandle(owner);owner=nullptr;lastSampleAt=0;ownerId=0; // unknown liveness also cannot prove death
 CursorRuntimeWatchdog();assert(held==mouseObject&&cursorState.state==1);
 cursorState.command=2;lastSampleAt=0;
 {gameentry::Scope scope(gameentry::Client::Cursor);assert(scope);tick();}
 assert(!held&&cursorState.state==0&&cursorRestoreCalls==1&&LONG(mouseObject[3])==0);
 NativeTooltipMaintain(false);assert(!tooltipOwned);
 cursorState.command=0;cursor(true);NativeTooltipMaintain(true);
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
 cursor(true);*(uint8_t*)nativetooltip::LayoutOriginal=0x90;NativeTooltipMaintain(true);
 assert(tooltipOwned&&!layoutOwned&&cursorState.tooltipState==-1);
 cursor(false,0);NativeTooltipMaintain(false);assert(NativeTooltipDetached());
 *(uint8_t*)nativetooltip::LayoutOriginal=nativetooltip::LayoutPrefix[0];
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
 cursorReadRecovery();

 // Run the real compiled frame adapter with live registers, EFLAGS (DF=1),
 // native store, and a balanced return continuation. No game simulation.
 frameobserver::continuation=uint32_t(uintptr_t(&frameReturn));
 framesignal::Channel c{};c.state=framesignal::Native;c.enabled=framesignal::Compute;c.thread=testThread;c.heartbeat=GetTickCount();
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
 // Exercise production maintain(): ordinary SP/LAN/Internet requests remain
 // on message polling; only an explicit high-risk diagnostic installs code.
 memcpy((void*)framesignal::CommitSite,frameobserver::original,6);
 *(uint32_t*)0xA83D4C=0x123456;*(uint8_t*)0xA8E9A0=1;*(uint8_t*)0xA8B8C2=0;
 for(int mode:{0,5,3,4}){
  *(int*)0xA8B238=mode;c.thread=testThread;c.heartbeat=GetTickCount();c.state=framesignal::Waiting;c.enabled=framesignal::Compute;
  frameobserver::channel=&c;frameobserver::owner=GetCurrentProcessId();patches=patchCalls;
  frameobserver::maintain(GetCurrentProcessId());assert(!frameOwned&&c.state==framesignal::MessageFallback&&patchCalls==patches);
  c.enabled|=framesignal::ExplicitDiagnostic;frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&c.state==framesignal::Native&&patchCalls==patches+1);
  patches=patchCalls;
  *(uint8_t*)0xA8E9A0=0;c.heartbeat=GetTickCount()-1000;
  frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&c.state==framesignal::Native&&patchCalls==patches);
  unsigned readyBeforePause=readyCalls;frameobserver::FrameSignalCommitted();assert(readyCalls==readyBeforePause);
  *(uint8_t*)0xA8E9A0=1;c.heartbeat=GetTickCount();
  frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&c.state==framesignal::Native&&patchCalls==patches);
  c.enabled=framesignal::Compute;frameobserver::maintain(GetCurrentProcessId());
  assert(!frameOwned&&c.state==framesignal::MessageFallback&&patchCalls==patches+1);
  c.enabled|=framesignal::ExplicitDiagnostic;frameobserver::maintain(GetCurrentProcessId());assert(frameOwned);
  // The fixture's channel is stack-owned; remove that pointer before the
  // real owner-exit cleanup path, which still restores the managed site.
  frameobserver::channel=nullptr;frameobserver::maintain(0);assert(!frameOwned&&!frameobserver::installed);
 }
 for(int mode:{-1,0,1,2,3,4,5,6}){
  bool supported=mode==0||mode==5||mode==3||mode==4;
  assert(!framesignal::nativeAllowed(mode,false)&&framesignal::nativeAllowed(mode,true)==supported);
 }
 // The manager records physical ownership before reporting protection/cache
 // errors. Failed installation after a write must still owe restoration.
 *(int*)0xA8B238=3;c.state=framesignal::Waiting;c.enabled=framesignal::Compute|framesignal::ExplicitDiagnostic;c.heartbeat=GetTickCount();
 frameobserver::channel=&c;frameobserver::owner=GetCurrentProcessId();failInstallAfterWrite=true;
 frameobserver::maintain(GetCurrentProcessId());
 assert(frameOwned&&frameobserver::installed&&c.state==framesignal::SignatureRejected);
 unsigned rejectedReady=readyCalls;frameobserver::FrameSignalCommitted();assert(readyCalls==rejectedReady);
 failInstallAfterWrite=false;failRestore=true;
 frameobserver::maintain(GetCurrentProcessId()+1);
 assert(frameOwned&&frameobserver::installed&&frameobserver::channel==&c&&frameobserver::owner==GetCurrentProcessId()&&c.state==framesignal::RestoreFailed);
 // Retrying an actual restore that reports an OS failure after committing
 // original bytes clears physical ownership, rather than poisoning retries.
 failRestore=false;failRestoreAfterWrite=true;c.enabled=framesignal::Compute;
 frameobserver::maintain(GetCurrentProcessId());
 assert(!frameOwned&&!frameobserver::installed&&!frameobserver::conflict&&c.state==framesignal::MessageFallback);
 failRestoreAfterWrite=false;c.enabled|=framesignal::ExplicitDiagnostic;
 frameobserver::maintain(GetCurrentProcessId());assert(frameOwned&&c.state==framesignal::Native);
 frameobserver::channel=nullptr;frameobserver::maintain(0);assert(!frameOwned&&!frameobserver::installed);
 puts("PASS frame write failures: post-write install failure retains ownership; failed restore retains old channel; post-write restore failure recovers without stale ownership");
 // Multiplayer still requires local playable state before NEW installation.
 *(int*)0xA8B238=3;c.state=framesignal::Waiting;c.enabled=framesignal::Compute|framesignal::ExplicitDiagnostic;c.heartbeat=GetTickCount();
 frameobserver::channel=&c;frameobserver::owner=GetCurrentProcessId();patches=patchCalls;
 *(uint8_t*)0xA8E9A0=0;frameobserver::maintain(GetCurrentProcessId());assert(!frameOwned&&patchCalls==patches);
 *(uint8_t*)0xA8E9A0=1;*(uint8_t*)0xA8B8C2=1;frameobserver::maintain(GetCurrentProcessId());assert(!frameOwned&&patchCalls==patches);
 *(uint8_t*)0xA8B8C2=0;c.enabled=0;frameobserver::maintain(GetCurrentProcessId());assert(!frameOwned&&patchCalls==patches);
 frameobserver::channel=nullptr;assert(!depth);
 puts("PASS frame adapter: GPR/ESP/EFLAGS/FXSAVE preserved; normal sessions use polling; explicit diagnostic installs; deselection and owner-exit restore");
 puts("PASS focus lifecycle: cursor hide/tooltip ownership retained; selected diagnostic frame hook survives pause/stale sample");
 puts("Synthetic Win32 execution only: no RA2OL/game visual or multiplayer claim.");
 return 0;
}
