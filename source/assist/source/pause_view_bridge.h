// Single-player pause is a native loop gate, never another command scheduler.
// The ordinary Runtime message entry continues to own all tool requests.
#include "pause_view_policy.h"
#include "hotkey_routes.h"
extern "C" unsigned __cdecl GuardedPauseViewMainGate();
extern "C" unsigned __cdecl GuardedPauseViewNative(unsigned,unsigned,unsigned,unsigned);
static pauseview::State pauseViewState;
static bool pauseViewInstalled=false,pauseViewNativeFailed=false;
static void pauseViewHotkeyReset();
static RiskSite pauseViewSite{experimentalpatch::PauseLoop,5,{0xA0,0xA0,0xE9,0xA8,0},{}};
static bool pauseViewAvailable(){
 if(pauseViewNativeFailed||!pauseViewInstalled||!sh||sh->stop||fault||!(sh->moFeatures&pauseview::Feature))return false;
 return pauseview::singleplayer(get<int>(0xA8B238))&&get<bool>(0xA8E9A0)&&
  sh->foreground&&get<bool>(0xA8ED80)&&!get<int>(0xA8EDA0)&&lastHouse&&
  lastHouse==get<uint32_t>(0xA83D4C)&&DWORD(sampleClock(sh)-sh->heartbeat)<3000;
}
// Losing focus or opening a modal menu resumes normal execution and cancels
// a pending step; neither transition retires the Hook.
static void pauseViewObserve(){
 pauseViewState.observe(pauseViewAvailable(),get<uint32_t>(0xA8B230),match,get<int>(0xA8ED84));
 if(sh)sh->pauseViewStatus=!(sh->moFeatures&pauseview::Feature)?0:pauseViewNativeFailed?-3:
  !pauseview::singleplayer(get<int>(0xA8B238))?-1:!pauseViewInstalled?-2:pauseViewState.overshot?-4:pauseViewState.step?3:pauseViewState.paused?2:1;
}
static void restorePauseView(){
 pauseViewState.reset();pauseViewHotkeyReset();pauseViewNativeFailed=false;
 const bool released=restoreRiskSite(pauseViewSite);pauseViewInstalled=!released;
 if(sh)sh->pauseViewStatus=released?0:-2;
}
// Use the same Press/Release/Double policy as ordinary configured hotkeys.
// This registry never registers another OS hook or a global key.
static hotkeyroute::Registry pauseViewHotkeys;
static uint32_t pauseViewKeyEpoch=0,pauseViewKeyValues[2]{};
static bool pauseViewKeySuppressed[256]{};
static void pauseViewHotkeyReset(){pauseViewHotkeys.clear();pauseViewKeyEpoch=0;}
static bool pauseViewKeyService(MSG*msg=nullptr){
 const bool keyDown=msg&&(msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN);
 const bool keyUp=msg&&(msg->message==WM_KEYUP||msg->message==WM_SYSKEYUP);
 const unsigned vk=msg?(unsigned(msg->wParam)&255):0;
 // A new physical press proves any older release was missed outside this
 // game input route. Preserve ownership across autorepeat and feature-off
 // only until that original release or the next genuinely new press.
 if(keyDown&&!(msg->lParam&(1u<<30)))pauseViewKeySuppressed[vk]=false;
 bool consumed=keyUp&&pauseViewKeySuppressed[vk];if(keyUp)pauseViewKeySuppressed[vk]=false;
 if(!pauseViewAvailable()||!sh->foreground||get<bool>(0xA8BC60+25)||get<int>(0xA8EDA0)){
  pauseViewHotkeys.cancel();return consumed;
 }
 bool changed=pauseViewKeyEpoch!=match;
 for(unsigned n=0;n<2;++n)changed|=pauseViewKeyValues[n]!=sh->pauseViewKeys[n];
 if(changed){
  pauseViewHotkeys.clear();pauseViewKeyEpoch=match;
  for(unsigned n=0;n<2;++n){pauseViewKeyValues[n]=sh->pauseViewKeys[n];
   pauseViewHotkeys.add(int(n+1),uint16_t(pauseViewKeyValues[n]),true,[](int,uint16_t){return true;});}
  pauseViewHotkeys.synchronized=true;
 }
 const uint32_t now=sampleClock(sh);uint8_t keys[256]{};
 for(unsigned n=0;n<256;++n)keys[n]=runtimeKeys.held[n];
 auto execute=[](hotkeyroute::Group actions){
  hotkeyroute::firstPerGesture(actions);
  for(int action:actions.actions){if(action==1)pauseViewState.toggle();else if(action==2)pauseViewState.once();}
  pauseViewObserve();
 };
 pauseViewHotkeys.sample(keys,now,match,execute,false);
 if(keyDown&&!(msg->lParam&(1u<<30))){
  const uint16_t code=keygesture::chord(vk,keys);
  for(const auto&group:pauseViewHotkeys.groups)if(group.key==code){
   consumed=pauseViewKeySuppressed[vk]=true;hotkeyroute::Group actions;
   if(pauseViewHotkeys.press(group.id,now,match,actions))execute(actions);break;
  }
 }
 hotkeyroute::Group delayed;
 while(pauseViewHotkeys.expired(now,match,delayed))execute(delayed);
 return consumed;
}
static bool pauseViewMessage(MSG*msg){
 if(!msg||!pauseViewKeyService(msg))return false;
 msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;
}
// Each non-pumping native draw/input operation owns a short Capture. The
// outer SEH wrapper releases that lease even when a native function faults.
extern "C" unsigned __cdecl AssistPauseViewNative(unsigned op,unsigned key,unsigned x,unsigned y){
 const uint32_t display=0x87F7E8;
 if(op==0){call<void>(0x4F42F0,display,2);return 1;}
 if(op==1)return call<int>(0x4F4320,display,(uint32_t*)uintptr_t(key),(int*)uintptr_t(x),(int*)uintptr_t(y));
 if(op==2){call<void>(0x4F4480,display);return 1;}
 if(op==3){const uint32_t tactical=get<uint32_t>(0x887324),vtable=tactical?get<uint32_t>(tactical):0;
  const uint32_t update=vtable?get<uint32_t>(vtable+0x5C):0;
  if(tactical>=0x10000&&update>=0x400000&&update<0x800000){call<void>(update,tactical);return 1;}
 }return 0;
}
// Return: 0=native loop, 1=paused/active, 2=paused/exit.
// No managed lease spans CallBack's native Windows message pump. Its ordinary
// Runtime message filters can therefore acquire the one existing manager.
extern "C" unsigned __cdecl AssistPauseViewMainGate(){
 pauseViewObserve();if(!pauseViewAvailable()||!pauseViewState.hold())return 0;
 if(get<bool>(0xA8E9A0)){
  ((void(__stdcall*)())uintptr_t(0x48D080))();
  pauseViewObserve();if(!pauseViewAvailable()||!pauseViewState.paused)return 0;
  if(pauseViewAvailable()&&get<bool>(0xA8ED80)&&!get<int>(0xA8EDA0)){
   uint32_t key=0;int x=0,y=0;
   GuardedPauseViewNative(0,0,0,0);if(pauseViewNativeFailed)return 0;
   GuardedPauseViewNative(1,uint32_t(uintptr_t(&key)),uint32_t(uintptr_t(&x)),uint32_t(uintptr_t(&y)));
   if(pauseViewNativeFailed)return 0;
   if(key){
    ((void(__fastcall*)(uint32_t*))uintptr_t(0x55DEE0))(&key);
    if(key==VK_ESCAPE||key==VK_SPACE)*(volatile int*)uintptr_t(0xA8EDA0)=1;
   }
   GuardedPauseViewNative(2,0,0,0);if(pauseViewNativeFailed)return 0;
   GuardedPauseViewNative(3,0,0,0);
  }
 }
 MsgWaitForMultipleObjects(0,nullptr,FALSE,15,QS_ALLINPUT);
 return get<bool>(0xA8E9A0)?1:2;
}
extern "C" void AssistPauseViewFailed(){pauseViewState.reset();pauseViewNativeFailed=true;if(sh)sh->pauseViewStatus=-3;}
static __declspec(naked) void pauseViewLoopEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 call GuardedPauseViewMainGate
 test eax,eax
 jz original
 dec eax
 mov [ebx+28],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 55DEDBh
 ret
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,byte ptr ds:[0A8E9A0h]
 push 55D365h
 ret
}}
static void pauseViewTick(){
 if(!sh||sh->stop||fault||!(sh->moFeatures&pauseview::Feature)){restorePauseView();return;}
 if(!pauseview::singleplayer(get<int>(0xA8B238))){restorePauseView();sh->pauseViewStatus=-1;return;}
 if(pauseViewNativeFailed){pauseViewInstalled=false;restoreRiskSite(pauseViewSite);sh->pauseViewStatus=-3;return;}
 if(!pauseViewSite.after[0]){pauseViewSite.after[0]=0xE9;const uint32_t offset=uint32_t(uintptr_t(&pauseViewLoopEntry))-pauseViewSite.address-5;memcpy(pauseViewSite.after+1,&offset,4);}
 pauseViewInstalled=installRiskSite(pauseViewSite);pauseViewObserve();pauseViewKeyService();
}
