#pragma once
#include <algorithm>
#include <cstdint>
namespace tabui {
enum class Mode { Toggle=0, Hold=1, TapOrHold=2 };
class Controller {
 bool down_=false,latched_=false;
 uint32_t pressedAt_=0,holdMs_=300;bool priorLatch_=false;
 Mode mode_=Mode::Toggle;
public:
 void mode(Mode m){mode_=m;reset();}
 void holdThreshold(uint32_t milliseconds){holdMs_=std::clamp(milliseconds,100u,2000u);}
 void reset(){down_=false;latched_=false;}
 void loseFocus(){down_=false;}
 bool visible()const{return mode_==Mode::Hold?down_:mode_==Mode::TapOrHold?(down_||latched_):latched_;}
 // Called only for Tab. Never eat Alt+Tab, injected input or keys in another app.
 bool event(bool down,bool game,bool modified,bool injected,uint32_t now=0){
  if(injected)return false;
  if(!game||modified){down_=false;return false;}
  if(down){if(!down_){pressedAt_=now;priorLatch_=latched_;if(mode_==Mode::Toggle)latched_=!latched_;}down_=true;return true;}
  bool owned=down_;if(owned&&mode_==Mode::TapOrHold)latched_=uint32_t(now-pressedAt_)>=holdMs_?false:!priorLatch_;
  down_=false;return owned;
 }
 void toggle(){if(mode_==Mode::Toggle)latched_=!latched_;}
};
inline bool displayRequested(bool hidden,bool always,bool manual,bool tabVisible){return !hidden&&(always||manual||tabVisible);}
inline void toggleDisplay(bool always,bool tabVisible,bool&hidden,bool&manual){
 bool shown=displayRequested(hidden,always,manual,tabVisible);hidden=shown;manual=!shown;
}
class Fade {
 int from_=0,to_=0;uint32_t at_=0,duration_=150;
public:
 int value(uint32_t now)const{
  auto elapsed=uint32_t(now-at_);if(elapsed>=duration_||!duration_)return to_;
  double t=double(elapsed)/duration_;t=t*t*(3.0-2.0*t);
  return std::clamp(int(from_+(to_-from_)*t+.5),0,255);
 }
 void target(bool visible,uint32_t now,uint32_t duration=150){
  int next=visible?255:0;if(next==to_)return;from_=value(now);to_=next;at_=now;duration_=duration;
 }
 void hide(){from_=to_=0;at_=0;}
 bool moving(uint32_t now)const{return from_!=to_&&uint32_t(now-at_)<duration_;}
};
}
