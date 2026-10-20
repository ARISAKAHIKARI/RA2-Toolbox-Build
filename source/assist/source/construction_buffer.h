#pragma once
// Host-only GDI backing store. WM_PAINT never exposes background clearing or
// partially drawn cards to the layered window. Keep the allocation between
// ticks; replace it only after a new size has been allocated successfully.
namespace assist { namespace construction {
class PanelBuffer {
 HDC memory{};HBITMAP bitmap{};HGDIOBJ previous{};int width=0,height=0;
public:
 PanelBuffer()=default;
 PanelBuffer(const PanelBuffer&)=delete;
 PanelBuffer& operator=(const PanelBuffer&)=delete;
 ~PanelBuffer(){clear();}
 void clear(){
  if(memory){SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);}
  memory=nullptr;bitmap=nullptr;previous=nullptr;width=height=0;
 }
 HDC acquire(HDC target,int w,int h){
  if(w<1||h<1)return nullptr;
  if(memory&&width==w&&height==h)return memory;
  HDC next=CreateCompatibleDC(target);if(!next)return nullptr;
  HBITMAP image=CreateCompatibleBitmap(target,w,h);
  if(!image){DeleteDC(next);return nullptr;}
  HGDIOBJ old=SelectObject(next,image);
  if(!old||old==HGDI_ERROR){DeleteObject(image);DeleteDC(next);return nullptr;}
  clear();memory=next;bitmap=image;previous=old;width=w;height=h;return memory;
 }
 bool present(HDC target){return memory&&BitBlt(target,0,0,width,height,memory,0,0,SRCCOPY);}
};
}}
