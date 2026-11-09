#pragma once
#include "native_hotkeys.h"
// One captured editor owns its real key releases. Reuse the native commit
// sender after all append packets have queued; never invent a second commit.
// Used by patrol and aircraft in the existing Runtime message boundary.
struct NativePlanningInput {
 HWND window=nullptr;uint32_t epoch=0,house=0;bool active=false,native=false,draining=false;
 nativekeys::Bindings bindings{};bool watched[256]{},primary[256]{};
 MSG releases[16]{};unsigned count=0,posted=0;
int identity(){
 if(!(*this).active||!(*this).window||(*this).epoch!=match||(*this).house!=lastHouse||
    !IsWindow((*this).window))return -1;
 uint32_t house=0;if(!read(0xA83D4C,&house,sizeof house))return 0;if(house!=(*this).house)return -1;
 DWORD target=0;GetWindowThreadProcessId((*this).window,&target);return target==GetCurrentProcessId()?1:-1;
}
bool sameContext(){
 if(identity()!=1)return false;DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);return foreground==GetCurrentProcessId();
}
void sampleKeys(uint8_t(&keys)[256]){for(unsigned key=0;key<256;++key)keys[key]=(GetKeyState(int(key))&0x8000)!=0;}
bool nativeHeld(const uint8_t*keys,uint16_t binding){
 const unsigned vk=binding&255,mods=(binding>>8)&7;
 return vk>=8&&keys[vk]&&(vk==16||bool(keys[16])==bool(mods&1))&&
  (vk==17||bool(keys[17])==bool(mods&2))&&(vk==18||bool(keys[18])==bool(mods&4));
}
bool watch(const uint8_t*keys){
 bool found=false;
 for(auto binding:(*this).bindings.keys[nativekeys::Planning])if(nativeHeld(keys,binding)){
  found=true;(*this).watched[binding&255]=(*this).primary[binding&255]=true;
  for(unsigned n=0;n<3;++n)if(binding&(1u<<(8+n)))(*this).watched[16+n]=true;
 }
 if(found)(*this).native=true;return found;
}
bool physicalHeld(){
 uint8_t keys[256]{};sampleKeys(keys);
 // A prefix may be released before the primary key. Wait for the primary's
 // real release as well, so native UI receives its commit-producing edge.
 for(unsigned vk=8;vk<256;++vk)if((*this).primary[vk]&&keys[vk])return true;
 return false;
}
bool replay(){
 if(!sameContext())return false;
 while((*this).posted<(*this).count){const auto&m=(*this).releases[(*this).posted];
  if(!PostMessageW(m.hwnd,m.message,m.wParam,m.lParam))return false;++(*this).posted;
 }return (*this).count>0;
}
void reset(){
 // A stale key-up must never submit a graph belonging to a different match,
 // House, recycled window or another foreground application.
 const int identity=this->identity();
 if(identity<0||(*this).posted==(*this).count){(*this)=NativePlanningInput{};return;}
 // These are actual input releases, not an invented Commit. If construction
 // was cancelled after submission they may commit a partial native plan.
 // Keep failed posts owned until this exact context can accept the release.
 (*this).draining=true;
 if(!identity)return;
 if(!physicalHeld())replay();
 if((*this).posted==(*this).count)(*this)=NativePlanningInput{};
}
bool capture(MSG*msg){
 reset();if((*this).active||!msg||!msg->hwnd||!IsWindow(msg->hwnd))return false;
 NativePlanningInput state{};state.window=msg->hwnd;state.epoch=match;state.house=lastHouse;state.active=true;
 state.bindings=nativekeys::load([](uint32_t at,void*out,size_t n){return read(at,out,n);});(*this)=state;
 if(!sameContext()){(*this)=NativePlanningInput{};return false;}
 uint8_t keys[256]{};sampleKeys(keys);const bool held=watch(keys);
 // A native editor with an unknown/released binding cannot be coordinated by
 // guessing a letter. A custom-only patrol does not depend on any native key.
 uint8_t editor=0;if(!read(0x87F7E8+0x11B3,&editor,1)||editor>1||(editor&&!held)){(*this)=NativePlanningInput{};return false;}
 return true;
}
void forgetRelease(unsigned vk){
 for(unsigned n=(*this).posted;n<(*this).count;){if((unsigned((*this).releases[n].wParam)&255)==vk){for(unsigned j=n+1;j<(*this).count;++j)(*this).releases[j-1]=(*this).releases[j];--(*this).count;}else ++n;}
}
bool message(MSG*msg,bool constructing){
 if(!msg||!(*this).active||(!constructing&&!(*this).draining))return false;
 const bool down=msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN;
 const bool up=msg->message==WM_KEYUP||msg->message==WM_SYSKEYUP;
 const int identity=this->identity();if(identity<0){(*this)=NativePlanningInput{};return false;}
 if(!identity){if(up&&msg->hwnd==(*this).window)forgetRelease(unsigned(msg->wParam)&255);return false;}
 if(!down&&!up)return false;const unsigned vk=unsigned(msg->wParam)&255;
 if((*this).draining){
  // A newer real release supersedes the deferred one and is passed through.
  if(up)forgetRelease(vk);
  reset();return false;
 }
 if(!sameContext())return false;
 uint8_t keys[256]{};sampleKeys(keys);
 // A native planning key pressed during construction also needs coordination.
 // On a release, reconstruct only that key's pre-release state for matching.
 if(up)keys[vk]=1;
 for(auto binding:(*this).bindings.keys[nativekeys::Planning])
  if((binding&255)==vk&&nativeHeld(keys,binding)){watch(keys);break;}
 if(!up||!(*this).native||!(*this).watched[vk])return false;
 // Keep the latest release per physical key. Re-pressing it before the graph
 // is ready must not replay an old release while the player is holding it.
 for(unsigned n=(*this).posted;n<(*this).count;++n)if((unsigned((*this).releases[n].wParam)&255)==vk){
  for(unsigned j=n+1;j<(*this).count;++j)(*this).releases[j-1]=(*this).releases[j];--(*this).count;break;
 }
 if((*this).count>=16)return false;
 (*this).releases[(*this).count++]=*msg;msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;
}
bool finish(uint32_t house){
 if(!sameContext()||house!=(*this).house)return false;
 uint8_t editor=0;if(!read(0x87F7E8+0x11B3,&editor,1)||editor>1)return false;
 if(!(*this).native&&editor){
  uint8_t keys[256]{};sampleKeys(keys);if(!watch(keys))return false;
 }
 if((*this).native){
  if(physicalHeld()||(*this).posted==(*this).count)return false;
  // Replaying the unchanged UI release invokes the original native commit
  // sender. Do not queue a second House-wide PlanCommit here.
  if(!replay())return false;(*this)=NativePlanningInput{};return true;
 }
 intent::EventOrder commit{};commit.domain=vanilla::Domain::NoOptions;
 commit.player=get<int>(house+0x30);commit.eventKind=vanilla::EventType::PlanCommit;
 if(!submitConstructed(commit))return false;(*this)=NativePlanningInput{};return true;
}

};
