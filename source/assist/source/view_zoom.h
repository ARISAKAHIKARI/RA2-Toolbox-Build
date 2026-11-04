#pragma once
#include <cstdint>
#include <cmath>
namespace assist { namespace viewzoom {
// MO's nine scale choices, plus the two previously saved Toolbox percentages.
// Zero means dynamic map fit, not division by zero or a disabled feature.
constexpr int Levels[]={0,50,67,75,80,100,125,150,200,300,400};
constexpr int Count=sizeof(Levels)/sizeof(Levels[0]);
inline int index(int percent){for(int i=0;i<Count;++i)if(Levels[i]==percent)return i;return 5;}
inline bool valid(int percent){return Levels[index(percent)]==percent;}
inline int step(int percent,int direction){int i=index(percent)+direction;return Levels[i<0?0:i>=Count?Count-1:i];}
inline const wchar_t* caption(int percent){return percent==0?L"全图（MAX）":percent==67?L"66.7%":nullptr;}
struct Size {int width=0,height=0;};
inline bool operator==(Size a,Size b){return a.width==b.width&&a.height==b.height;}
inline bool operator!=(Size a,Size b){return !(a==b);}
inline bool validSize(Size s){return s.width>=640&&s.height>=480&&s.width<=8192&&s.height<=8192&&int64_t(s.width)*s.height<=40000000;}
inline Size target(Size base,int percent){
 if(!percent||!valid(percent)||!validSize(base))return {};
 Size s{int((int64_t(base.width)*100+percent/2)/percent),int((int64_t(base.height)*100+percent/2)/percent)};
 return validSize(s)?s:Size{};
}
inline bool validRaster(Size s){return s.width>=64&&s.height>=64&&s.width<=8192&&s.height<=8192&&int64_t(s.width)*s.height<=40000000;}
inline Size nativeTarget(Size area,int percent,Size map={}){
 if(!valid(percent)||!validRaster(area))return {};
 double scale=percent==67?1.5:percent?100./percent:0.;
 if(!percent){
  if(map.width<16||map.height<16||map.width>1000000||map.height>1000000)return {};
  scale=std::fmax(double(map.width)/area.width,double(map.height)/area.height);
  if(!std::isfinite(scale)||scale<=0.02||scale>64)return {};
  const double dimension=8192./std::fmax(area.width,area.height);
  const double pixels=std::sqrt(40000000./(double(area.width)*area.height));
  scale=std::fmin(scale,std::fmin(dimension,pixels));
 }
 Size result{int(std::floor(area.width*scale+0.5)),int(std::floor(area.height*scale+0.5))};
 // Rounding two independently capped axes upward can exceed the exact pixel
 // budget. MAX may round down by less than one pixel per axis to stay valid.
 if(!percent&&!validRaster(result))result={int(std::floor(area.width*scale)),int(std::floor(area.height*scale))};
 return validRaster(result)?result:Size{};
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
