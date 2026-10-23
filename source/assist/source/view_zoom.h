#pragma once
#include <cstdint>
namespace assist { namespace viewzoom {
constexpr int Levels[]={50,75,100,125,150};
inline int index(int percent){for(int i=0;i<5;++i)if(Levels[i]==percent)return i;return 2;}
inline bool valid(int percent){return Levels[index(percent)]==percent;}
inline int step(int percent,int direction){int i=index(percent)+direction;return Levels[i<0?0:i>4?4:i];}
struct Size {int width=0,height=0;};
inline bool operator==(Size a,Size b){return a.width==b.width&&a.height==b.height;}
inline bool operator!=(Size a,Size b){return !(a==b);}
inline bool validSize(Size s){return s.width>=640&&s.height>=480&&s.width<=8192&&s.height<=8192&&int64_t(s.width)*s.height<=40000000;}
inline Size target(Size base,int percent){
 if(!valid(percent)||!validSize(base))return {};
 Size s{int((int64_t(base.width)*100+percent/2)/percent),int((int64_t(base.height)*100+percent/2)/percent)};
 return validSize(s)?s:Size{};
}
// The existing host input/overlay mapping is a full-client linear transform.
// Reject letterboxing rather than silently pointing at a different cell.
inline bool sameAspect(Size a,Size b){
 if(a.width<=0||a.height<=0||b.width<=0||b.height<=0)return false;
 int64_t d=int64_t(a.width)*b.height-int64_t(b.width)*a.height;if(d<0)d=-d;
 return d*500<=int64_t(a.width)*b.height; // <= 0.2%, including rounded dimensions
}
enum Status {Off=0,Applied=1,Waiting=2,Renderer=-1,Native=-2,Dimensions=-3,ResizeFailed=-4,RestoreFailed=-5,Presentation=-6,Fault=-7};
}}
