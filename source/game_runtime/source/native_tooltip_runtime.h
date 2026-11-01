#pragma once
#include "../../shared/source/native_tooltip_client.h"
#include "../../shared/source/runtime_fp_scope.h"
extern "C" bool CursorTooltipNeeded();
extern "C" bool CursorTooltipOwnsHiddenMouse();
extern "C" unsigned GuardedBuildTimeTooltip(unsigned,unsigned);
namespace nativetooltip {
static bool installed=false,buildTimeWanted=false;
constexpr uint32_t Slot=0x7F74D8,Original=0x479050,Sidebar=0x87F7E8,Provider=0x6D1800;
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
static bool compatible(){
 uint8_t actual[sizeof(NativeBody)]{},getter[4]{};SIZE_T got=0;
 const uint8_t expectedGetter[]={0x8B,0x41,0x0C,0xC3};
 return ReadProcessMemory(GetCurrentProcess(),(const void*)Original,actual,sizeof actual,&got)&&got==sizeof actual&&
  !memcmp(actual,NativeBody,sizeof actual)&&
  ReadProcessMemory(GetCurrentProcess(),(const void*)0x7B89F0,getter,sizeof getter,&got)&&got==sizeof getter&&
  !memcmp(getter,expectedGetter,sizeof getter);
}
static wchar_t* __fastcall entry(void*self,void*,unsigned id){
 auto text=((wchar_t*(__thiscall*)(void*,unsigned))Original)(self,id);
 if(!text&&CursorTooltipNeeded()){
  bool ours=false;
  {RuntimeFPScope fp;ours=compatible()&&CursorTooltipOwnsHiddenMouse();}
  // Only OUR one held hide may be discounted. A second engine hide, a
  // different mouse object or an unknown wrapper retains the native result.
  // Do not write RefCount, ShowCursor, HideName, RNG or gameplay objects.
  if(ours)text=((wchar_t*(__thiscall*)(void*,unsigned))Provider)((void*)Sidebar,id);
 }
 if(!text||!buildTimeWanted)return text;
 RuntimeFPScope fp;
 return reinterpret_cast<wchar_t*>(uintptr_t(GuardedBuildTimeTooltip(id,unsigned(uintptr_t(text)))));
}
static bool maintain(bool allowInstall){
 const bool wanted=buildTimeWanted||CursorTooltipNeeded();
 const uint32_t target=uint32_t(uintptr_t(&entry));
 if(wanted&&!installed&&allowInstall&&compatible()){
  const bool ok=gameentry::patch(gameentry::Client::Assist,Slot,(const uint8_t*)&Original,(const uint8_t*)&target,4);
  // The writer records actual bytes even if flush/protection restoration
  // failed. Retain ownership in that case so cleanup can retry.
  installed=ok||gameentry::inspect(Slot).active;
 }else if(!wanted&&installed){
  if(gameentry::patch(gameentry::Client::Assist,Slot,(const uint8_t*)&target,(const uint8_t*)&Original,4))installed=false;
  else installed=gameentry::inspect(Slot).active;
 }
 return installed;
}
}
extern "C" bool NativeTooltipBuildTime(bool wanted){
 nativetooltip::buildTimeWanted=wanted;nativetooltip::maintain(wanted);
 return wanted?nativetooltip::installed:(!nativetooltip::installed||CursorTooltipNeeded());
}
extern "C" void NativeTooltipMaintain(bool allowInstall){
 gameentry::Scope entry(gameentry::Client::Assist);if(entry)nativetooltip::maintain(allowInstall);
}
extern "C" bool NativeTooltipDetached(){return !nativetooltip::installed&&!nativetooltip::buildTimeWanted;}
