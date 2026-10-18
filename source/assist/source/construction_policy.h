#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include "../../shared/source/viewport_scale.h"
namespace assist { namespace construction {
constexpr int Capacity=5,PanelWidth=144,MaxHeaderHeight=188,RowHeight=66,PanelOpacity=216;
constexpr int StackTop=24,StackStep=24,StackHeight=48;
inline bool prerequisite(int index,const std::vector<int>&owned,const std::vector<int>*groups,int future=-1){
 auto has=[&](int n){return n>=0&&n<int(owned.size())&&(owned[n]>0||n==future);};
 if(index>=0)return has(index);if(index< -6)return false;
 for(int n:groups[-index-1])if(has(n))return true;return false;
}
inline bool available(const std::vector<int>&needs,const std::vector<int>&overrides,const std::vector<int>&owned,const std::vector<int>*groups,int future=-1){
 // Original PrerequisiteOverride uses building indices, not generic groups.
 for(int n:overrides)if(n>=0&&prerequisite(n,owned,groups,future))return true;
 for(int n:needs)if(!prerequisite(n,owned,groups,future))return false;return true;
}
inline bool countryAllowed(uint32_t owner,uint32_t required,uint32_t forbidden,int parent,int country){
 return parent>=0&&parent<32&&country>=0&&country<32&&(owner&(1u<<parent))&&
  (required==UINT32_MAX||(required&(1u<<country)))&&
  (forbidden==UINT32_MAX||!(forbidden&(1u<<country)));
}
struct Book {
 uint32_t match=0,house=0,source=0,id=0,token=0,sent=0,retry=0,missing=0;
 int stage=0,attempts=0,placedFrame=-1;std::vector<int> items;
 bool add(int index){if(index<0||items.size()>=Capacity)return false;items.push_back(index);return true;}
 int head()const{return items.empty()?-1:items.front();}
 void consume(uint32_t p,uint32_t identity){items.erase(items.begin());source=p;id=identity;stage=attempts=0;sent=retry=missing=token=0;placedFrame=-1;}
 bool afterPlacement(int frame,bool ready){if(!ready||frame<0){placedFrame=-1;return false;}if(placedFrame<0||frame<placedFrame){placedFrame=frame;return false;}return frame>placedFrame;}
};
// All geometry is in game pixels. Convert both painting AND hit testing with
// the same transform; monitor dimensions and Windows DPI are not game pixels.
struct Layout {
 int gameWidth=0,gameHeight=0,clientWidth=0,clientHeight=0,rows=0,scalePercent=100,queueCount=0,choiceCount=0;
 float zoom=1;
 bool set(int w,int h,int cw,int ch,int requestedPercent=100){if(w<640||h<400||w>32768||h>32768||cw<1||ch<1||cw>32768||ch>32768)return false;
  gameWidth=w;gameHeight=h;clientWidth=cw;clientHeight=ch;scalePercent=viewport::percent(requestedPercent);
  // Fit at least one choice row in a small viewport; keep the saved preference.
  zoom=std::min(viewport::factor(scalePercent),std::min(float(w-168)/PanelWidth,float(h-8)/(MaxHeaderHeight+RowHeight)));
  content(queueCount,choiceCount);return true;}
 int stackBottom()const{return StackTop+(queueCount?StackHeight+(queueCount-1)*StackStep:32);}
 int headerHeight()const{return stackBottom()+20;}
 void content(int queued,int candidates){
  queueCount=std::clamp(queued,0,Capacity);choiceCount=std::max(0,candidates);rows=0;
  if(!gameHeight||!choiceCount)return;
  const int header=headerHeight();
  float anchor=std::min(200.f,std::max(0.f,gameHeight-8-(header+RowHeight)*zoom));
  int available=std::clamp(int(((gameHeight-anchor-8)/zoom-header)/RowHeight),1,6);
  rows=std::min(available,(choiceCount+1)/2);
 }
 int x(int v)const{return int(std::lround(double(v)*clientWidth/gameWidth*zoom));}
 int y(int v)const{return int(std::lround(double(v)*clientHeight/gameHeight*zoom));}
 int width()const{return x(PanelWidth);}int height()const{return y(headerHeight()+rows*RowHeight);}
 int left()const{return std::max(0,int(std::lround(double(gameWidth-168)*clientWidth/gameWidth))-width());}
 int top()const{return std::max(0,std::min(int(std::lround(200.0*clientHeight/gameHeight)),clientHeight-height()-int(std::ceil(8.0*clientHeight/gameHeight))));}
 int queueAt(int px,int py,int count=Capacity)const{if(px<x(5)||px>=x(139))return -1;for(int i=std::min(count,Capacity)-1;i>=0;--i)if(py>=y(StackTop+i*StackStep)&&py<y(StackTop+i*StackStep+StackHeight))return i;return -1;}
 int choiceAt(int px,int py,int scroll,int count)const{
  if(px<0||px>=width()||py<y(headerHeight())||py>=height())return -1;
  int row=0;while(row<rows&&py>=y(headerHeight()+(row+1)*RowHeight))++row;
  int n=scroll+row*2+(px>=x(72));return row<rows&&n<count?n:-1;
 }
};
}}
