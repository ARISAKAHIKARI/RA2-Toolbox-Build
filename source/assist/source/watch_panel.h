#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include "construction_buffer.h"
#include "watch_presentation.h"
namespace watchpanel {
// Keep the successfully rasterized front between WM_PAINT messages. Failed
// replacement cannot overwrite it; exposures copy it without rerunning GDI+.
class Buffer {
 assist::construction::PanelBuffer front_,spare_;watchpresentation::Gate gate_;
 int width_=0,height_=0;bool ready_=false;
public:
 watchpresentation::Metric stages,copies;
 void clear(){front_.clear();spare_.clear();gate_.invalidate();width_=height_=0;ready_=false;}
 bool needs(const watchpresentation::Visual&v)const{return !ready_||gate_.needs(v);}
 bool ready(int w,int h)const{return ready_&&width_==w&&height_==h;}
 bool stage(HDC target,const std::shared_ptr<const std::vector<uint32_t>>&pixels,const watchpresentation::Visual&v){
  if(!needs(v))return true;const double started=watchpresentation::now();
  auto done=[&](bool ok){stages.add(watchpresentation::now()-started,ok);return ok;};
  if(!target||!pixels||pixels.get()!=v.image.get()||v.gpu||v.sourceWidth<1||v.sourceHeight<1||v.sourceWidth>8192||v.sourceHeight>8192||v.width<1||v.height<1||v.width>8192||v.height>8192||uint64_t(v.width)*v.height>16*1024*1024||pixels->size()!=size_t(v.sourceWidth)*v.sourceHeight)return done(false);
  HDC memory=spare_.acquire(target,v.width,v.height);if(!memory)return done(false);
  {
   Gdiplus::Graphics g(memory);Gdiplus::Bitmap image(v.sourceWidth,v.sourceHeight,v.sourceWidth*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(pixels->data())));
   if(g.GetLastStatus()!=Gdiplus::Ok||image.GetLastStatus()!=Gdiplus::Ok)return done(false);
   g.Clear(Gdiplus::Color(255,0,0,0));g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
   g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
   g.ScaleTransform(float(v.width)/640,float(v.height)/512);
   if(g.DrawImage(&image,Gdiplus::RectF(0,0,640,512),0.f,0.f,float(v.sourceWidth),float(v.sourceHeight),Gdiplus::UnitPixel,nullptr)!=Gdiplus::Ok)return done(false);
   Gdiplus::Pen border(Gdiplus::Color(255,v.dangerous?255:105,v.dangerous?167:195,v.dangerous?62:225),6.f);
   if(g.DrawRectangle(&border,3.f,3.f,634.f,506.f)!=Gdiplus::Ok)return done(false);
   g.Flush(Gdiplus::FlushIntentionSync);
  }
  if(!GdiFlush())return done(false);
  front_.swap(spare_);width_=v.width;height_=v.height;ready_=true;gate_.committed(v);return done(true);
 }
 bool present(HDC target,int w,int h){const double started=watchpresentation::now();const bool ok=ready(w,h)&&front_.present(target);copies.add(watchpresentation::now()-started,ok);return ok;}
};
}
