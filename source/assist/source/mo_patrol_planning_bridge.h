#pragma once
#include "native_hotkeys.h"
// Include after the MO gesture state. A planning event carries its own append
// byte; a physical planning key is neither synthesized nor required here.
// The actual native key, when the player is using it, keeps commit ownership.
struct MoPlanningUI {
 HWND window=nullptr;uint32_t epoch=0,house=0;bool active=false,native=false,draining=false;
 nativekeys::Bindings bindings{};bool watched[256]{},primary[256]{};
 MSG releases[16]{};unsigned count=0,posted=0;
};
static MoPlanningUI moPlanningUI;
static int moPlanningIdentity(){
 if(!moPlanningUI.active||!moPlanningUI.window||moPlanningUI.epoch!=match||moPlanningUI.house!=lastHouse||
    !IsWindow(moPlanningUI.window))return -1;
 uint32_t house=0;if(!read(0xA83D4C,&house,sizeof house))return 0;if(house!=moPlanningUI.house)return -1;
 DWORD target=0;GetWindowThreadProcessId(moPlanningUI.window,&target);return target==GetCurrentProcessId()?1:-1;
}
static bool moPlanningSameContext(){
 if(moPlanningIdentity()!=1)return false;DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);return foreground==GetCurrentProcessId();
}
static void moPlanningKeys(uint8_t(&keys)[256]){for(unsigned key=0;key<256;++key)keys[key]=(GetKeyState(int(key))&0x8000)!=0;}
static bool moPlanningNativeHeld(const uint8_t*keys,uint16_t binding){
 const unsigned vk=binding&255,mods=(binding>>8)&7;
 return vk>=8&&keys[vk]&&(vk==16||bool(keys[16])==bool(mods&1))&&
  (vk==17||bool(keys[17])==bool(mods&2))&&(vk==18||bool(keys[18])==bool(mods&4));
}
static bool moPlanningWatch(const uint8_t*keys){
 bool found=false;
 for(auto binding:moPlanningUI.bindings.keys[nativekeys::Planning])if(moPlanningNativeHeld(keys,binding)){
  found=true;moPlanningUI.watched[binding&255]=moPlanningUI.primary[binding&255]=true;
  for(unsigned n=0;n<3;++n)if(binding&(1u<<(8+n)))moPlanningUI.watched[16+n]=true;
 }
 if(found)moPlanningUI.native=true;return found;
}
static bool moPlanningPhysicalHeld(){
 uint8_t keys[256]{};moPlanningKeys(keys);
 // A prefix may be released before the primary key. Wait for the primary's
 // real release as well, so native UI receives its commit-producing edge.
 for(unsigned vk=8;vk<256;++vk)if(moPlanningUI.primary[vk]&&keys[vk])return true;
 return false;
}
static bool moPlanningReplay(){
 if(!moPlanningSameContext())return false;
 while(moPlanningUI.posted<moPlanningUI.count){const auto&m=moPlanningUI.releases[moPlanningUI.posted];
  if(!PostMessageW(m.hwnd,m.message,m.wParam,m.lParam))return false;++moPlanningUI.posted;
 }return moPlanningUI.count>0;
}
static void moPlanningReset(){
 // A stale key-up must never submit a graph belonging to a different match,
 // House, recycled window or another foreground application.
 const int identity=moPlanningIdentity();
 if(identity<0||moPlanningUI.posted==moPlanningUI.count){moPlanningUI=MoPlanningUI{};return;}
 // These are actual input releases, not an invented Commit. If construction
 // was cancelled after submission they may commit a partial native plan.
 // Keep failed posts owned until this exact context can accept the release.
 moPlanningUI.draining=true;
 if(!identity)return;
 if(!moPlanningPhysicalHeld())moPlanningReplay();
 if(moPlanningUI.posted==moPlanningUI.count)moPlanningUI=MoPlanningUI{};
}
static bool moPlanningCapture(MSG*msg){
 moPlanningReset();if(moPlanningUI.active||!msg||!msg->hwnd||!IsWindow(msg->hwnd))return false;
 MoPlanningUI state{};state.window=msg->hwnd;state.epoch=match;state.house=lastHouse;state.active=true;
 state.bindings=nativekeys::load([](uint32_t at,void*out,size_t n){return read(at,out,n);});moPlanningUI=state;
 if(!moPlanningSameContext()){moPlanningUI=MoPlanningUI{};return false;}
 uint8_t keys[256]{};moPlanningKeys(keys);const bool held=moPlanningWatch(keys);
 // A native editor with an unknown/released binding cannot be coordinated by
 // guessing a letter. A custom-only patrol does not depend on any native key.
 uint8_t editor=0;if(!read(0x87F7E8+0x11B3,&editor,1)||editor>1||(editor&&!held)){moPlanningUI=MoPlanningUI{};return false;}
 return true;
}
static void moPlanningForgetRelease(unsigned vk){
 for(unsigned n=moPlanningUI.posted;n<moPlanningUI.count;){if((unsigned(moPlanningUI.releases[n].wParam)&255)==vk){for(unsigned j=n+1;j<moPlanningUI.count;++j)moPlanningUI.releases[j-1]=moPlanningUI.releases[j];--moPlanningUI.count;}else ++n;}
}
static bool moPlanningMessage(MSG*msg){
 if(!msg||!moPlanningUI.active||(!moHolding&&!moStage&&!moPlanningUI.draining))return false;
 const bool down=msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN;
 const bool up=msg->message==WM_KEYUP||msg->message==WM_SYSKEYUP;
 const int identity=moPlanningIdentity();if(identity<0){moPlanningUI=MoPlanningUI{};return false;}
 if(!identity){if(up&&msg->hwnd==moPlanningUI.window)moPlanningForgetRelease(unsigned(msg->wParam)&255);return false;}
 if(!down&&!up)return false;const unsigned vk=unsigned(msg->wParam)&255;
 if(moPlanningUI.draining){
  // A newer real release supersedes the deferred one and is passed through.
  if(up)moPlanningForgetRelease(vk);
  moPlanningReset();return false;
 }
 if(!moPlanningSameContext())return false;
 uint8_t keys[256]{};moPlanningKeys(keys);
 // A native planning key pressed during construction also needs coordination.
 // On a release, reconstruct only that key's pre-release state for matching.
 if(up)keys[vk]=1;
 for(auto binding:moPlanningUI.bindings.keys[nativekeys::Planning])
  if((binding&255)==vk&&moPlanningNativeHeld(keys,binding)){moPlanningWatch(keys);break;}
 if(!up||!moPlanningUI.native||!moPlanningUI.watched[vk])return false;
 // Keep the latest release per physical key. Re-pressing it before the graph
 // is ready must not replay an old release while the player is holding it.
 for(unsigned n=moPlanningUI.posted;n<moPlanningUI.count;++n)if((unsigned(moPlanningUI.releases[n].wParam)&255)==vk){
  for(unsigned j=n+1;j<moPlanningUI.count;++j)moPlanningUI.releases[j-1]=moPlanningUI.releases[j];--moPlanningUI.count;break;
 }
 if(moPlanningUI.count>=16)return false;
 moPlanningUI.releases[moPlanningUI.count++]=*msg;msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;
}
static bool moPlanningFinish(uint32_t house){
 if(!moPlanningSameContext()||house!=moPlanningUI.house)return false;
 uint8_t editor=0;if(!read(0x87F7E8+0x11B3,&editor,1)||editor>1)return false;
 if(!moPlanningUI.native&&editor){
  uint8_t keys[256]{};moPlanningKeys(keys);if(!moPlanningWatch(keys))return false;
 }
 if(moPlanningUI.native){
  if(moPlanningPhysicalHeld()||moPlanningUI.posted==moPlanningUI.count)return false;
  // Replaying the unchanged UI release invokes the original native commit
  // sender. Do not queue a second House-wide PlanCommit here.
  if(!moPlanningReplay())return false;moPlanningUI=MoPlanningUI{};return true;
 }
 intent::EventOrder commit{};commit.domain=vanilla::Domain::NoOptions;
 commit.player=get<int>(house+0x30);commit.eventKind=vanilla::EventType::PlanCommit;
 if(!submitConstructed(commit))return false;moPlanningUI=MoPlanningUI{};return true;
}
