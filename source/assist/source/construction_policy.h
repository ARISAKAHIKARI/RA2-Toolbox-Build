#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
namespace assist { namespace construction {
constexpr int Capacity=5,PanelWidth=144,HeaderHeight=116,RowHeight=66;
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
 int stage=0,attempts=0;std::vector<int> items;
 bool add(int index){if(index<0||items.size()>=Capacity)return false;items.push_back(index);return true;}
 int head()const{return items.empty()?-1:items.front();}
 void consume(uint32_t p,uint32_t identity){items.erase(items.begin());source=p;id=identity;stage=attempts=0;sent=retry=missing=token=0;}
};
// All geometry is in game pixels. Convert both painting AND hit testing with
// the same transform; monitor dimensions and Windows DPI are not game pixels.
struct Layout {
 int gameWidth=0,gameHeight=0,clientWidth=0,clientHeight=0,rows=1;
 bool set(int w,int h,int cw,int ch){if(w<640||h<400||w>32768||h>32768||cw<1||ch<1||cw>32768||ch>32768)return false;
  gameWidth=w;gameHeight=h;clientWidth=cw;clientHeight=ch;rows=std::clamp((h-200-HeaderHeight-8)/RowHeight,1,6);return true;}
 int x(int v)const{return int((int64_t(v)*clientWidth+gameWidth/2)/gameWidth);}
 int y(int v)const{return int((int64_t(v)*clientHeight+gameHeight/2)/gameHeight);}
 int width()const{return x(PanelWidth);}int height()const{return y(HeaderHeight+rows*RowHeight);}
 int left()const{return x(std::max(0,gameWidth-160-PanelWidth-8));}int top()const{return y(200);}
 int queueAt(int px,int py)const{if(px<0||px>=width()||py<y(24)||py>=y(114))return -1;for(int i=0;i<Capacity;++i)if(py<y(24+(i+1)*18))return i;return -1;}
 int choiceAt(int px,int py,int scroll,int count)const{
  if(px<0||px>=width()||py<y(HeaderHeight)||py>=height())return -1;
  int row=0;while(row<rows&&py>=y(HeaderHeight+(row+1)*RowHeight))++row;
  int n=scroll+row*2+(px>=x(72));return row<rows&&n<count?n:-1;
 }
};
}}
