#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace assist { namespace formation {
constexpr int Maximum=256, Shapes=4;
constexpr double Pi=3.14159265358979323846;
struct Policy {uint32_t enabled=0,hold=18,keys[4]={'Z','X','C','V'};};
struct Unit {uint32_t id=0,pointer=0,type=0,token=0,color=0;int x=0,y=0,z=0,kind=0;};
struct Slot {int x=0,y=0,z=0;uint32_t valid=0;};
struct Preview {uint32_t match=0,tick=0,active=0,dragging=0,revision=0;int shape=0,count=0,ax=0,ay=0;double facing=0;Unit units[Maximum]{};Slot slots[Maximum]{};};
// Shared keys deliberately need not be distinct. Only a new physical press
// calls this function, so OS key-repeat never advances a group.
inline int cycle(int current,uint32_t key,const uint32_t*keys){
 int first=-1,next=-1;bool found=false;
 for(int i=0;i<Shapes;++i)if(keys[i]&&keys[i]==key){if(first<0)first=i;if(found&&next<0)next=i;if(i==current)found=true;}
 return next>=0?next:first>=0?first:current;
}
struct Point {double x=0,y=0;};
inline void layout(int count,int shape,double stretch,bool click,Point*out){
 count=std::clamp(count,0,Maximum);shape=std::clamp(shape,0,3);stretch=std::clamp(stretch,0.,1.);
 if(!count)return;
 // A click always makes the tightest square footprint. All distances are
 // world cells; dispersed mode adds one empty cell between occupied cells.
 double spacing=(!click&&shape==1)?2.:1.;
 int base=std::max(1,int(std::ceil(std::sqrt(count*(click?1.:std::sqrt(2.))))));
 int columns=click?base:std::clamp(int(std::lround(base+(count-base)*stretch)),1,count);
 if(click||shape<2){int rows=(count+columns-1)/columns;
  for(int n=0;n<count;++n){int row=n/columns,width=std::min(columns,count-row*columns);out[n]={(n%columns-(width-1)*.5)*spacing,(row-(rows-1)*.5)*spacing};}
 }else if(shape==2){
  // Hexagonal rings keep the centre filled, instead of a hollow circle.
  out[0]={};int n=1;
  for(int ring=1;n<count;++ring){int members=std::min(6*ring,count-n);for(int j=0;j<members;++j){double a=2*Pi*j/members;out[n++]={ring*std::cos(a),ring*std::sin(a)};}}
 }else{
  // 60 degree sector of an annulus. arc length / radial depth is about 3.
  int rows=std::max(1,int(std::lround(std::sqrt(count/3.))));int n=0;
  for(int row=0;row<rows;++row){int width=(count-n+rows-row-1)/(rows-row);double radius=std::max(1.,3.*rows/(Pi/3))+(row-(rows-1)*.5);
   for(int j=0;j<width;++j){double angle=width==1?0:(double(j)/(width-1)-.5)*Pi/3;out[n++]={radius*std::sin(angle),-radius*std::cos(angle)};}}
 }
 if(!click&&shape>=2&&stretch>0){
  // Continuous drag still ends at a single horizontal rank for every shape.
  for(int i=1;i<count;++i){Point v=out[i];int j=i;while(j>0&&out[j-1].x>v.x){out[j]=out[j-1];--j;}out[j]=v;}
  for(int n=0;n<count;++n){out[n].x=out[n].x*(1-stretch)+(n-(count-1)*.5)*stretch;out[n].y*=1-stretch;}
 }
 double x=0,y=0;for(int n=0;n<count;++n){x+=out[n].x;y+=out[n].y;}x/=count;y/=count;
 for(int n=0;n<count;++n){out[n].x-=x;out[n].y-=y;}
}
// Stable nearest-slot assignment reduces crossing. Tie breaks use selection
// order, never game RNG. Caller has already checked and de-duplicated cells.
inline void assign(const Unit*units,int count,Slot*slots){
 bool used[Maximum]{};Slot assigned[Maximum]{};
 for(int n=0;n<count;++n){int best=-1;double distance=1e100;for(int j=0;j<count;++j)if(!used[j]){double dx=double(units[n].x)-slots[j].x,dy=double(units[n].y)-slots[j].y,d=dx*dx+dy*dy;if(d<distance){distance=d;best=j;}}if(best>=0){used[best]=true;assigned[n]=slots[best];}}
 for(int n=0;n<count;++n)slots[n]=assigned[n];
}
}}
