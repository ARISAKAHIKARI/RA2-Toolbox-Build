#pragma once
#include <stdint.h>
// Pure bounded geometry. One map cell is 256 leptons on either map axis.
namespace assist {namespace reversedodge {
constexpr int MergeFrames=12; // Fixed window from the accepted Move; never slides.
struct Point{int x=0,y=0;};
inline unsigned root(unsigned n){unsigned r=0,bit=1u<<30;while(bit>n)bit>>=2;while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}return r;}
inline bool vector(double vx,double vy,Point&out){
 if(!(vx==vx&&vy==vy)||vx>1e9||vx< -1e9||vy>1e9||vy< -1e9)return false;
 double ax=vx<0?-vx:vx,ay=vy<0?-vy:vy,m=ax>ay?ax:ay;if(m<0.000001)return false;
 int x=int(vx/m*1024),y=int(vy/m*1024);unsigned len=root(unsigned(x*x+y*y));if(!len)return false;
 out={-x*512/int(len),-y*512/int(len)};return true;
}
inline int direction(int x,int y){int ax=x<0?-x:x,ay=y<0?-y:y;if(ax*2<ay)return y<0?0:4;if(ay*2<ax)return x>0?2:6;return y<0?(x>0?1:7):(x>0?3:5);}
template<class CanEnter> inline uint32_t destination(int px,int py,Point delta,CanEnter canEnter){
 struct Candidate{int x,y,score,dir;};Candidate candidates[49];int count=0;
 if(px<0||py<0||px>=512*256||py>=512*256)return 0;
 for(int y=py/256-3;y<=py/256+3;++y)for(int x=px/256-3;x<=px/256+3;++x){
  if(x<=0||y<=0||x>=512||y>=512)continue;
  int dx=x*256+128-px,dy=y*256+128-py,d2=dx*dx+dy*dy;
  int dot=dx*delta.x+dy*delta.y,cross=dx*delta.y-dy*delta.x;
  if(d2<320*320||d2>704*704||dot<=0||cross>dot||cross< -dot)continue;
  int ex=dx-delta.x,ey=dy-delta.y,score=ex*ex+ey*ey;
  int at=count++;while(at>0&&score<candidates[at-1].score){candidates[at]=candidates[at-1];--at;}
  candidates[at]={x,y,score,direction(dx,dy)};
 }
 for(int i=0;i<count;++i){const auto&c=candidates[i];if(canEnter(c.x,c.y,c.dir))return uint32_t(c.y*1000+c.x);}
 return 0;
}
}}
