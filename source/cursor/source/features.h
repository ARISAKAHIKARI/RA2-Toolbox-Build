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
struct Burst {
 int left=0;bool armed=false;int x=0,y=0;uint32_t due=0;
 void cancel(){left=0;armed=false;}
 void down(int px,int py,bool eligible){cancel();armed=eligible;x=px;y=py;}
 void up(int px,int py,bool eligible,uint32_t now,unsigned gap){if(armed&&eligible&&std::abs(px-x)<=4&&std::abs(py-y)<=4){left=4;due=now+gap;}armed=false;}
 bool ready(uint32_t now)const{return left>0&&int32_t(now-due)>=0;}
 void sent(uint32_t now,unsigned gap){if(left)--left;due=now+gap;}
};
