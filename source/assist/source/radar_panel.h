#pragma once
#include "construction_buffer.h"
#include "radar_paint_gate.h"
#include "../../shared/source/gpu_compositor.h"
// Stage the entire GDI image before changing a panel's geometry or showing it.
// WM_PAINT normally copies this completed front; exposures/periodic repairs do
// not re-run HALFTONE or allocate a brush for every unit marker.
namespace radarpanel {
// Only this panel needs direct pixels after its GDI resize. Keep both complete
// DIBs alive, so an annotation/resize failure never replaces the visible front.
class PixelBuffer {
 HDC memory_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ previous_=nullptr;uint32_t*pixels_=nullptr;int width_=0,height_=0;
public:
 PixelBuffer()=default;PixelBuffer(const PixelBuffer&)=delete;PixelBuffer&operator=(const PixelBuffer&)=delete;
 ~PixelBuffer(){clear();}
 void clear(){if(memory_){SelectObject(memory_,previous_);DeleteObject(bitmap_);DeleteDC(memory_);}memory_=nullptr;bitmap_=nullptr;previous_=nullptr;pixels_=nullptr;width_=height_=0;}
 void swap(PixelBuffer&o){using std::swap;swap(memory_,o.memory_);swap(bitmap_,o.bitmap_);swap(previous_,o.previous_);swap(pixels_,o.pixels_);swap(width_,o.width_);swap(height_,o.height_);}
 HDC acquire(HDC target,int w,int h){if(memory_&&width_==w&&height_==h)return memory_;HDC dc=CreateCompatibleDC(target);if(!dc)return nullptr;
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  void*bits=nullptr;HBITMAP image=CreateDIBSection(target,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!image){DeleteDC(dc);return nullptr;}HGDIOBJ old=SelectObject(dc,image);if(!old||old==HGDI_ERROR){DeleteObject(image);DeleteDC(dc);return nullptr;}
  clear();memory_=dc;bitmap_=image;previous_=old;pixels_=static_cast<uint32_t*>(bits);width_=w;height_=h;return memory_;
 }
 uint32_t*pixels(){return pixels_;}
 bool present(HDC target){return memory_&&BitBlt(target,0,0,width_,height_,memory_,0,0,SRCCOPY);}
};
class Gpu {
 gpupresent::Compositor compositor_;int width_=0,height_=0;
public:
 bool active=false,pending=false;
 void reset(){compositor_.reset();width_=height_=0;active=pending=false;}
 bool ready(int w,int h)const{return active&&width_==w&&height_==h;}
 void cpu(HWND window){if(!active)return;reset();auto style=GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_NOREDIRECTIONBITMAP;
  SetWindowLongPtrW(window,GWL_EXSTYLE,style&~WS_EX_LAYERED);SetWindowLongPtrW(window,GWL_EXSTYLE,style|WS_EX_LAYERED);SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
 }
 template<class Image>bool present(HWND window,const Image&image,const radargate::Visual&v){
  if(!image.gpu||!v.real||!v.marks.empty())return false;
  if(!active){SetWindowLongPtrW(window,GWL_EXSTYLE,(GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_LAYERED)|WS_EX_NOREDIRECTIONBITMAP);active=true;}
  overlaycomposition::Rect view{v.view.left,v.view.top,v.view.right-v.view.left,v.view.bottom-v.view.top};
  const auto glyphs=panelglyphs::place(image.ranks,v.width,v.height);
  pending=!compositor_.presentFrame(window,image.gpu,v.width,v.height,0xff9da173u,&view,&glyphs);
  if(!pending){width_=v.width;height_=v.height;}else if(compositor_.reason()!="GPU frame not ready")gpupresent::available()=false;
  return !pending;
 }
};
class Buffer {
 PixelBuffer front_,spare_;
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
  // Flush pending GDI writes before touching DIB pixels. Insignia is added at
  // this panel's final dimensions, then its camera rectangle/border stays top.
  if(!GdiFlush())return false;panelglyphs::paint(spare_.pixels(),v.width,v.height,panelglyphs::place(image.ranks,v.width,v.height));
  if(v.view.right>v.view.left&&v.view.bottom>v.view.top){RECT r{v.view.left,v.view.top,v.view.right,v.view.bottom};if(SetDCBrushColor(memory,RGB(230,234,205))==CLR_INVALID||!FrameRect(memory,&r,brush))return false;}
  RECT border{0,0,v.width,v.height};if(SetDCBrushColor(memory,RGB(157,161,115))==CLR_INVALID||!FrameRect(memory,&border,brush)||!GdiFlush())return false;
  pendingWidth_=v.width;pendingHeight_=v.height;pending_=true;return true;
 }
 bool promote(){if(!pending_)return false;front_.swap(spare_);width_=pendingWidth_;height_=pendingHeight_;ready_=true;pending_=false;return true;}
 bool present(HDC target,int w,int h){painted=ready(w,h)&&front_.present(target);return painted;}
};
}
