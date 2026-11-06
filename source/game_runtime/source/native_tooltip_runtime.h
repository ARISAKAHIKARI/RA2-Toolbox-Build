#pragma once
#include "../../shared/source/native_tooltip_client.h"
#include "../../shared/source/runtime_fp_scope.h"
#include "../../shared/source/debug_entry_policy.h"
extern "C" bool CursorTooltipNeeded();
extern "C" bool CursorTooltipOwnsHiddenMouse();
extern "C" void CursorTooltipStatus(int);
extern "C" unsigned GuardedBuildTimeTooltip(unsigned,unsigned);
namespace nativetooltip {
static bool installed=false,layoutInstalled=false,buildTimeWanted=false;
constexpr uint32_t Slot=0x7F74D8,Original=0x479050,Sidebar=0x87F7E8,Provider=0x6D1800;
constexpr uint32_t LayoutSlot=0x7F74C8,LayoutOriginal=0x478BA0,LayoutVisible=0x478BBA;
static uint32_t layoutContinuation=LayoutVisible;
// Supported EXE SHA-256: 268a81b7...fbb8d0. The complete native wrapper:
// GetRefCount; negative => nullptr; otherwise Sidebar.GetToolTip(id).
// Verify the receiver, argument forwarding and callee cleanup as well as the
// visibility check before using its native positive branch directly.
static constexpr uint8_t NativeBody[]={
 0x8B,0x0D,0x40,0x76,0x88,0,0x8B,0x01,0xFF,0x50,0x28,
 0x85,0xC0,0x7D,5,0x33,0xC0,0xC2,4,0,
 0x8B,0x4C,0x24,4,0x51,0xB9,0xE8,0xF7,0x87,0,
 0xE8,0x8D,0x87,0x25,0,0xC2,4,0
};
// CCToolTip::OnToolTip / vtable+4 has a SECOND visibility check after the
// text provider. Returning false here makes ToolTipManager discard the box.
static constexpr uint8_t LayoutPrefix[]={
 0x83,0xEC,0x1C,0x89,0x4C,0x24,0x08,
 0x8B,0x0D,0x40,0x76,0x88,0,0x8B,0x01,0xFF,0x50,0x28,
 0x85,0xC0,0x0F,0x8C,0xEC,0x01,0,0,
 0xA0,0x7C,0xEB,0xA8,0,0x8B,0x0D,0xD0,0xC4,0x89,0
};
static bool layoutCompatible(){
 uint8_t actual[sizeof(LayoutPrefix)]{},tail[8]{};SIZE_T got=0;
 const uint8_t expectedTail[]={0x32,0xC0,0x83,0xC4,0x1C,0xC2,4,0};
 const bool headRead=ReadProcessMemory(GetCurrentProcess(),(const void*)LayoutOriginal,actual,sizeof actual,&got)&&got==sizeof actual;
 const bool tailRead=ReadProcessMemory(GetCurrentProcess(),(const void*)0x478DA6,tail,sizeof tail,&got)&&got==sizeof tail;
 if(!debugentries::allow(debugentries::Kind::Patch,LayoutSlot,headRead&&tailRead,
    !memcmp(actual,LayoutPrefix,sizeof actual)&&!memcmp(tail,expectedTail,sizeof tail)))return false;
 // visibleLayout replays these locals/receiver stores and returns through
 // this exact native epilogue. A checkbox cannot relocate an unknown ABI.
 if(!memcmp(actual,LayoutPrefix,7)&&!memcmp(tail,expectedTail,sizeof tail))return true;
 debugentries::installation(debugentries::Kind::Patch,LayoutSlot,debugentries::Hook::Unavailable,debugentries::Reason::Structural);return false;
}
// Reproduce exactly the stack/local setup skipped with the negative-count
// branch. ECX and the one callee-cleaned rectangle argument are unchanged;
// the native positive path performs all sizing, clipping and positioning.
static bool __declspec(naked) __thiscall visibleLayout(void*,void*){
 __asm {
  sub esp,0x1c
  mov dword ptr [esp+8],ecx
  xor eax,eax
  jmp dword ptr [layoutContinuation]
 }
}
static bool __fastcall layoutEntry(void*self,void*,void*rectangle){
 bool ours=false;
 {RuntimeFPScope fp;runtimeunwind::run([&]{ours=CursorTooltipNeeded()&&layoutCompatible()&&CursorTooltipOwnsHiddenMouse();},[&]{fp.restore();});}
 if(ours)return visibleLayout(self,rectangle);
 return ((bool(__thiscall*)(void*,void*))LayoutOriginal)(self,rectangle);
}
static bool compatible(){
 uint8_t actual[sizeof(NativeBody)]{},getter[4]{};SIZE_T got=0;
 const uint8_t expectedGetter[]={0x8B,0x41,0x0C,0xC3};
 const bool bodyRead=ReadProcessMemory(GetCurrentProcess(),(const void*)Original,actual,sizeof actual,&got)&&got==sizeof actual;
 const bool getterRead=ReadProcessMemory(GetCurrentProcess(),(const void*)0x7B89F0,getter,sizeof getter,&got)&&got==sizeof getter;
 return debugentries::allow(debugentries::Kind::Patch,Slot,bodyRead&&getterRead,
   !memcmp(actual,NativeBody,sizeof actual)&&!memcmp(getter,expectedGetter,sizeof getter));
}
static wchar_t* __fastcall entry(void*self,void*,unsigned id){
 auto text=((wchar_t*(__thiscall*)(void*,unsigned))Original)(self,id);
 if(!text&&CursorTooltipNeeded()){
  bool ours=false;
  {RuntimeFPScope fp;runtimeunwind::run([&]{ours=compatible()&&CursorTooltipOwnsHiddenMouse();},[&]{fp.restore();});}
  // Only OUR one held hide may be discounted. A second engine hide, a
  // different mouse object or an unknown wrapper retains the native result.
  // Do not write RefCount, ShowCursor, HideName, RNG or gameplay objects.
  if(ours)text=((wchar_t*(__thiscall*)(void*,unsigned))Provider)((void*)Sidebar,id);
 }
 if(!text||!buildTimeWanted)return text;
 RuntimeFPScope fp;wchar_t* result=nullptr;
 runtimeunwind::run([&]{result=reinterpret_cast<wchar_t*>(uintptr_t(GuardedBuildTimeTooltip(id,unsigned(uintptr_t(text)))));},[&]{fp.restore();});
 return result;
}
static bool maintain(bool allowInstall){
 const bool cursorWanted=CursorTooltipNeeded(),wanted=buildTimeWanted||cursorWanted;
 const uint32_t target=uint32_t(uintptr_t(&entry));
 // A per-slot policy change retires through the old ownership journal before
 // any new admission. Unrelated entry settings do not churn these callbacks.
 static bool policySeen=false,textRequired=true,layoutRequired=true;
 const bool nextText=debugentries::signatureRequired(debugentries::Kind::Patch,Slot);
 const bool nextLayout=debugentries::signatureRequired(debugentries::Kind::Patch,LayoutSlot);
 bool textPending=false,layoutPending=false;
 if(policySeen&&textRequired!=nextText&&installed){
  const auto journal=gameentry::inspect(Slot);
  if(journal.active&&journal.length==4)gameentry::patch(gameentry::Client::Assist,Slot,journal.after,journal.before,4);
  installed=gameentry::inspect(Slot).active!=0;textPending=installed;
 }
 if(policySeen&&layoutRequired!=nextLayout&&layoutInstalled){
  const auto journal=gameentry::inspect(LayoutSlot);
  if(journal.active&&journal.length==4)gameentry::patch(gameentry::Client::Assist,LayoutSlot,journal.after,journal.before,4);
  layoutInstalled=gameentry::inspect(LayoutSlot).active!=0;layoutPending=layoutInstalled;
 }
 policySeen=true;if(!textPending)textRequired=nextText;if(!layoutPending)layoutRequired=nextLayout;
 if(textPending||layoutPending){CursorTooltipStatus(cursorWanted?-1:0);return installed;}
 if(wanted&&!installed&&allowInstall&&compatible()){
  const bool ok=gameentry::patch(gameentry::Client::Assist,Slot,(const uint8_t*)&Original,(const uint8_t*)&target,4);
  // The writer records actual bytes even if flush/protection restoration
  // failed. Retain ownership in that case so cleanup can retry.
  installed=ok||gameentry::inspect(Slot).active;
 }else if(!wanted&&installed){
  if(gameentry::patch(gameentry::Client::Assist,Slot,(const uint8_t*)&target,(const uint8_t*)&Original,4))installed=false;
  else installed=gameentry::inspect(Slot).active;
 }
 const uint32_t layoutTarget=uint32_t(uintptr_t(&layoutEntry));
 if(cursorWanted&&!layoutInstalled&&allowInstall&&compatible()&&layoutCompatible()){
  const bool ok=gameentry::patch(gameentry::Client::Assist,LayoutSlot,(const uint8_t*)&LayoutOriginal,(const uint8_t*)&layoutTarget,4);
  layoutInstalled=ok||gameentry::inspect(LayoutSlot).active;
 }else if(!cursorWanted&&layoutInstalled){
  if(gameentry::patch(gameentry::Client::Assist,LayoutSlot,(const uint8_t*)&layoutTarget,(const uint8_t*)&LayoutOriginal,4))layoutInstalled=false;
  else layoutInstalled=gameentry::inspect(LayoutSlot).active;
 }
 // Physical ownership is retained even on a failed flush/restore. Readiness
 // additionally requires the actual slot to remain ours (foreign writes are
 // never overwritten). An unavailable repair is visible to the controller.
 uint32_t textActual=0,layoutActual=0;SIZE_T got=0;
 const bool ready=cursorWanted&&installed&&layoutInstalled&&compatible()&&layoutCompatible()&&
  ReadProcessMemory(GetCurrentProcess(),(const void*)Slot,&textActual,4,&got)&&got==4&&textActual==target&&
  ReadProcessMemory(GetCurrentProcess(),(const void*)LayoutSlot,&layoutActual,4,&got)&&got==4&&layoutActual==layoutTarget;
 CursorTooltipStatus(cursorWanted?(ready?1:-1):0);
 return installed;
}
}
extern "C" bool NativeTooltipBuildTime(bool wanted){
 nativetooltip::buildTimeWanted=wanted;nativetooltip::maintain(wanted);
 return wanted?nativetooltip::installed:(!nativetooltip::installed||CursorTooltipNeeded());
}
extern "C" void NativeTooltipMaintain(bool allowInstall){
 gameentry::scoped(gameentry::Client::Assist,false,[&]{nativetooltip::maintain(allowInstall);});
}
extern "C" bool NativeTooltipDetached(){return !nativetooltip::installed&&!nativetooltip::layoutInstalled&&!nativetooltip::buildTimeWanted;}
