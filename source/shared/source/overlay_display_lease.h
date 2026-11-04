#pragma once
#include <cstdint>
namespace overlaydisplay {
struct Context {
 uintptr_t window=0;uint32_t pid=0,match=0;
 int clientWidth=0,clientHeight=0,nativeWidth=0,nativeHeight=0;
 bool valid()const{return window&&pid&&match&&clientWidth>0&&clientHeight>0&&nativeWidth>0&&nativeHeight>0;}
 bool operator==(const Context&o)const{return window==o.window&&pid==o.pid&&match==o.match&&clientWidth==o.clientWidth&&clientHeight==o.clientHeight&&nativeWidth==o.nativeWidth&&nativeHeight==o.nativeHeight;}
};
enum class State {Hidden,Hold,Ready};
// A failed information/display read is not proof that the battlefield closed.
// Hold only existing pixels, never use this result to authorize a game command.
// Only a fully readable visible state with a valid frame renews the deadline.
class ReadLease {
 Context context_{};uint32_t tick_=0;bool valid_=false;
public:
 static constexpr uint32_t Grace=120;
 void reset(){valid_=false;}
 State inspect(const Context&context,bool eligible,bool displayReadable,bool battlefieldVisible,bool frameReady,uint32_t now){
  if(!eligible||!context.valid()||(displayReadable&&!battlefieldVisible)){reset();return State::Hidden;}
  if(displayReadable&&battlefieldVisible&&frameReady){context_=context;tick_=now;valid_=true;return State::Ready;}
  if(valid_&&context_==context&&uint32_t(now-tick_)<Grace)return State::Hold;
  reset();return State::Hidden;
 }
};
}
