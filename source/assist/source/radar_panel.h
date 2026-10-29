#pragma once
#include "construction_buffer.h"
#include "radar_paint_gate.h"
// Stage the entire GDI image before changing a panel's geometry or showing it.
// WM_PAINT normally copies this completed front; exposures/periodic repairs do
// not re-run HALFTONE or allocate a brush for every unit marker.
namespace radarpanel {
class Buffer {
 assist::construction::PanelBuffer front_,spare_;
 int width_=0,height_=0,pendingWidth_=0,pendingHeight_=0;
 bool ready_=false,pending_=false;
public:
 bool painted=false;
 void clear(){front_.clear();spare_.clear();width_=height_=pendingWidth_=pendingHeight_=0;ready_=pending_=painted=false;}
 bool ready(int w,int h)const{return ready_&&w==width_&&h==height_;}
 template<class Image>bool stage(HDC target,const Image&image,const radargate::Visual&v){
  pending_=false;
  if(!target||v.width<3||v.height<3||v.width>8192||v.height>8192||uint64_t(v.width)*v.height>16*1024*1024||image.width<1||image.height<1||image.width>2048||image.height>2048||image.pixels.size()!=size_t(image.width)*image.height)return false;
  HDC memory=spare_.acquire(target,v.width,v.height);if(!memory)return false;
  const int w=v.width-2,h=v.height-2;
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=image.width;info.bmiHeader.biHeight=-image.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  if(image.width==w&&image.height==h){if(SetDIBitsToDevice(memory,1,1,w,h,0,0,0,h,image.pixels.data(),&info,DIB_RGB_COLORS)!=UINT(h))return false;}
  else{if(!SetStretchBltMode(memory,HALFTONE)||!SetBrushOrgEx(memory,0,0,nullptr))return false;const int rows=StretchDIBits(memory,1,1,w,h,0,0,image.width,image.height,image.pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);if(!rows||rows==GDI_ERROR)return false;}
  HBRUSH brush=static_cast<HBRUSH>(GetStockObject(DC_BRUSH));if(!brush)return false;
  for(const auto&m:v.marks){RECT r{m.left,m.top,m.right,m.bottom};if(SetDCBrushColor(memory,RGB(m.color>>16,(m.color>>8)&255,m.color&255))==CLR_INVALID||!FillRect(memory,&r,brush))return false;}
  if(v.view.right>v.view.left&&v.view.bottom>v.view.top){RECT r{v.view.left,v.view.top,v.view.right,v.view.bottom};if(SetDCBrushColor(memory,RGB(230,234,205))==CLR_INVALID||!FrameRect(memory,&r,brush))return false;}
  RECT border{0,0,v.width,v.height};if(SetDCBrushColor(memory,RGB(157,161,115))==CLR_INVALID||!FrameRect(memory,&border,brush)||!GdiFlush())return false;
  pendingWidth_=v.width;pendingHeight_=v.height;pending_=true;return true;
 }
 bool promote(){if(!pending_)return false;front_.swap(spare_);width_=pendingWidth_;height_=pendingHeight_;ready_=true;pending_=false;return true;}
 bool present(HDC target,int w,int h){painted=ready(w,h)&&front_.present(target);return painted;}
};
}
