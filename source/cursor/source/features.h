#pragma once
#include <cstdint>
#include <algorithm>
#include <cstdlib>
struct Tween {
 int frame=-1,group=-1,from=-1;uint32_t at=0;
 void reset(){frame=group=from=-1;at=0;}
 void update(int next,int key,int first,int count,uint32_t now,bool enabled){
  if(next==frame&&key==group)return;
  bool consecutive=frame>=first&&frame<first+count&&next==first+(frame-first+1)%std::max(1,count);
  from=enabled&&key>=0&&group==key&&count>1&&consecutive?frame:-1;
  frame=next;group=key;at=now;
 }
 unsigned weight(uint32_t now,unsigned duration)const{return from<0?255:std::min(255u,(uint32_t(now-at)*255u)/std::max(1u,duration));}
};
inline uint32_t blendPixel(uint32_t a,uint32_t b,unsigned t){uint32_t v=0;for(int sh=0;sh<32;sh+=8){unsigned x=(a>>sh)&255,y=(b>>sh)&255;v|=((x*(255-t)+y*t+127)/255)<<sh;}return v;}
#include "../../shared/source/click_burst.h"
using Burst=clickinput::Burst;
