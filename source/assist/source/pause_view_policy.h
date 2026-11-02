#pragma once
#include <stdint.h>
namespace assist { namespace pauseview {
constexpr uint32_t Feature=16;
inline bool singleplayer(int session){return session==0||session==5;}
struct State {
 bool paused=false,step=false,overshot=false;uint32_t scenario=0,epoch=0;int frame=-1,stepFrame=-1;
 void reset(){paused=step=overshot=false;scenario=epoch=0;frame=stepFrame=-1;}
 void observe(bool available,uint32_t scene,uint32_t match,int current){
  if(!available||!scene||current<0){reset();return;}
  if((scenario&&scenario!=scene)||(epoch&&epoch!=match)||(frame>=0&&current<frame))paused=step=overshot=false;
  scenario=scene;epoch=match;frame=current;
 }
 void toggle(){paused=!paused;step=overshot=false;}
 void once(){if(paused&&!step){step=true;overshot=false;stepFrame=frame;}}
 bool hold(){
  if(!paused)return false;
  if(step){if(frame<=stepFrame)return false;overshot=int64_t(frame)>int64_t(stepFrame)+1;step=false;stepFrame=-1;}
  return true;
 }
};
}}
