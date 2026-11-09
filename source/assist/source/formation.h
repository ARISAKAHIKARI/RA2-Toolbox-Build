#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../../shared/source/key_gesture.h"
namespace assist { namespace formation {
constexpr int Maximum=1024, Shapes=6;
constexpr double Pi=3.14159265358979323846;
struct Policy {uint32_t enabled=0,hold=18,keys[Shapes]={'Z','X','C','V','B','N'},mouse=2;};
struct Unit {uint32_t id=0,pointer=0,type=0,token=0,color=0;int x=0,y=0,z=0,kind=0;};
struct Slot {int x=0,y=0,z=0;uint32_t valid=0;};
struct Preview {uint32_t match=0,tick=0,active=0,dragging=0,revision=0;int shape=0,count=0,ax=0,ay=0;double facing=0;Unit units[Maximum]{};Slot slots[Maximum]{};};
// Shape changes are discrete presses; release/hold modes are not accepted.
// Legacy presses and explicit Press share one identity, including EXT keys.
inline uint16_t shapeKey(uint32_t key){
 const auto mode=keygesture::mode(key);
 return (key&255)&&(mode==keygesture::Mode::Legacy||mode==keygesture::Mode::Press)?keygesture::normalize(key):0;
}
// Shared keys deliberately need not be distinct. Only a new physical press
// calls this function, so OS key-repeat never advances a group.
inline int cycle(int current,uint32_t key,const uint32_t*keys){
 key=shapeKey(key);if(!key)return current;
 int first=-1,next=-1;bool found=false;
 for(int i=0;i<Shapes;++i)if(shapeKey(keys[i])==key){if(first<0)first=i;if(found&&next<0)next=i;if(i==current)found=true;}
 return next>=0?next:first>=0?first:current;
}
struct Point {double x=0,y=0;};
// Inverse of the shared 30:15 isometric projection. Keep sub-cell drag
// direction; snapping the pointer to a map cell must not snap the facing.
inline double dragFacing(double x,double y,double fallback){return x||y?std::atan2(2*y-x,2*y+x):fallback;}
inline void layout(int count,int shape,double stretch,bool click,Point*out){
 count=std::clamp(count,0,Maximum);shape=std::clamp(shape,0,Shapes-1);stretch=click?0:std::clamp(stretch,0.,1.);
 if(!count)return;
 // A click previews the selected shape at rest; only a drag stretches it.
 // Distances are world cells; dispersed mode adds one empty cell.
 double spacing=shape==1?2.:1.;
 int base=std::max(1,int(std::ceil(std::sqrt(count*(shape==4?1.:std::sqrt(2.))))));
 int columns=std::clamp(int(std::lround(base+(count-base)*stretch)),1,count);
 if(shape==5){
  // At rest, occupied border area : hollow area is approximately 3:1.
  // Increasing the square footprint with a fixed member count thins the
  // border. The final footprint has only one occupied perimeter layer.
  int compact=std::max(int(std::ceil(std::sqrt(count))),int(std::lround(std::sqrt(count*4./3.))));
  if(count>=4)compact=std::max(3,compact);if(count>=8&&compact*compact==count)++compact;
  int widest=std::max(compact,(count+7)/4);
  int side=std::max(1,int(std::lround(compact+(widest-compact)*stretch))),n=0;
  for(int inset=0;n<count;++inset){int edge=side-2*inset;if(edge<=1){out[n++]={0,0};break;}
   int perimeter=4*(edge-1),take=std::min(perimeter,count-n);double half=(edge-1)*.5;
   for(int j=0;j<take;++j){int p=int((int64_t(2*j+1)*perimeter)/(2*take)),segment=p/(edge-1),at=p%(edge-1);Point q;
    if(segment==0)q={-half+at,-half};else if(segment==1)q={half,-half+at};else if(segment==2)q={half-at,half};else q={-half,half-at};out[n++]=q;
   }
  }
 }else if(shape<2||shape==4){int rows=(count+columns-1)/columns;
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
 if(!click&&(shape==2||shape==3)&&stretch>0){
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
 count=std::clamp(count,0,Maximum);
 bool used[Maximum]{};Slot assigned[Maximum]{};
 for(int n=0;n<count;++n){int best=-1;double distance=1e100;for(int j=0;j<count;++j)if(!used[j]&&slots[j].valid){double dx=double(units[n].x)-slots[j].x,dy=double(units[n].y)-slots[j].y,d=dx*dx+dy*dy;if(d<distance){distance=d;best=j;}}if(best>=0){used[best]=true;assigned[n]=slots[best];}}
 for(int n=0;n<count;++n)slots[n]=assigned[n];
}
}}
