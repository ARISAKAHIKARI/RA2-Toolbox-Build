#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <algorithm>
#include <cstdint>
#include "../../external_reader/source/unit_info_panel_model.h"
#include "../../shared/source/overlay_stack.h"
#include "unit_info_panel_cameos.h"

// RA2 Toolbox's original unit panel implementation. No MO source/assets,
// engine hooks, game events or game-memory reads live in this UI object.
// Only page/collapse/detail interactions are accepted in v1. The popup HWND
// consumes panel clicks, unlike a WS_EX_TRANSPARENT world overlay.
namespace unitpanelhost {
class Panel {
 HWND window_=nullptr;
 external::unitpanel::Model model_{};
 uint64_t signature_=0;DWORD lastGoodAt_=0;unitpanelcameo::Cache cameos_{};uint32_t cameoRevision_=0;
 bool folded_=false,details_=false;
 int page_=0,focused_=-1,width_=390,height_=40;
 static constexpr int Header=38,Footer=30;
 static std::wstring name(const std::array<uint16_t,40>&src){
  std::wstring value;for(uint16_t c:src){if(!c)break;if(c>=32)value.push_back(wchar_t(c));}
  return value.empty()?L"未知单位":value;
 }
 static constexpr int CellW=60,CellH=48,ButtonW=26,Pad=7,Gap=1;
 int entries()const{return model_.selected==1?model_.passengerCount:model_.groupCount;}
 int perPage()const{return model_.selected==1?7:9;}
 int pages()const{return external::unitpanel::pageCount(entries(),perPage());}
 int slots()const{return model_.selected==1?2+std::min(7,std::max(0,entries()-page_*perPage())):
   std::min(9,std::max(0,entries()-page_*perPage()));}
 int cols()const{int n=slots();return n<=1?1:n<=4?2:3;}
 int rows()const{return std::max(1,(slots()+cols()-1)/cols());}
 int wantedWidth()const{
  if(folded_)return ButtonW+3;
  return ButtonW+Pad*2+4+cols()*CellW+(cols()-1)*Gap;
 }
 int wantedHeight()const{
  if(folded_)return ButtonW+3;
  return Pad*2+rows()*CellH+(rows()-1)*Gap+(details_?42:0)+(pages()>1?22:0);
 }
 static void label(Gdiplus::Graphics&g,const std::wstring&s,float x,float y,float size,
                   Gdiplus::Color color,bool strong=false){
  if(s.empty())return;
  Gdiplus::FontFamily family(L"Microsoft YaHei UI");
  Gdiplus::Font font(&family,size,strong?Gdiplus::FontStyleBold:Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
  Gdiplus::SolidBrush brush(color);
  Gdiplus::StringFormat format;format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
  g.DrawString(s.c_str(),-1,&font,Gdiplus::PointF(x,y),&format,&brush);
 }
 static Gdiplus::Color tint(int kind){
  switch(kind){case 0:return {255,128,189,255};case 1:return {255,132,218,163};
   case 2:return {255,226,174,115};case 3:return {255,193,151,225};
   default:return {255,170,180,190};}
 }
 static void box(Gdiplus::Graphics&g,float x,float y,float w,float h,Gdiplus::Color c){
  Gdiplus::SolidBrush b(c);g.FillRectangle(&b,x,y,w,h);
 }
 static void outline(Gdiplus::Graphics&g,int x,int y,int w,int h,Gdiplus::Color color){
  Gdiplus::Pen pen(color,1);g.DrawRectangle(&pen,x,y,w-1,h-1);
 }
 void cameo(Gdiplus::Graphics&g,uint32_t type,int kind,int x,int y,
            const std::array<uint16_t,40>&nm){
  if(const auto*icon=cameos_.get(type)){
   auto*buf=reinterpret_cast<BYTE*>(const_cast<uint32_t*>(icon->argb.data()));
   Gdiplus::Bitmap bmp(um::IW,um::IH,um::IW*4,PixelFormat32bppARGB,buf);
   g.DrawImage(&bmp,Gdiplus::Rect(x,y,CellW,CellH),0,0,um::IW,um::IH,Gdiplus::UnitPixel);
  }else{
   box(g,float(x),float(y),CellW,CellH,Gdiplus::Color(255,32,42,52));
   box(g,float(x+3),float(y+3),CellW-6,CellH-6,tint(kind));
   label(g,name(nm).substr(0,1),float(x+20),float(y+10),20,
         Gdiplus::Color(255,21,28,35),true);
  }
 }
 void paint(Gdiplus::Graphics&g){
  using namespace Gdiplus;
  g.SetSmoothingMode(SmoothingModeNone);
  g.SetInterpolationMode(InterpolationModeNearestNeighbor);
  g.SetPixelOffsetMode(PixelOffsetModeHalf);
  g.Clear(Color(255,10,12,15));
  // Original MO-style compact 60x48 cameo grid: close spacing, bevel border,
  // and a left-side fold/details button column. Artwork is never copied.
  const int fx=2,fy=height_-ButtonW-1;
  box(g,float(fx),float(fy),ButtonW-2,ButtonW-2,Color(255,47,55,66));
  outline(g,fx,fy,ButtonW-2,ButtonW-2,Color(255,159,165,175));
  label(g,folded_?L"▸":L"◂",float(fx+5),float(fy+2),17,
        Color(255,230,222,147),true);
  if(folded_)return;
  if(height_>=ButtonW*2+6){
   int dy=fy-ButtonW-2;
   box(g,float(fx),float(dy),ButtonW-2,ButtonW-2,
       details_?Color(255,80,89,100):Color(255,40,47,57));
   outline(g,fx,dy,ButtonW-2,ButtonW-2,Color(255,133,148,164));
   label(g,L"i",float(fx+9),float(dy+3),15,Color(255,244,240,214),true);
  }
  int bx=ButtonW+4,by=Pad-2;
  int gw=Pad*2+cols()*CellW+(cols()-1)*Gap;
  int gh=Pad*2+rows()*CellH+(rows()-1)*Gap;
  box(g,float(bx),float(by),float(gw),float(gh),Color(255,11,14,18));
  outline(g,bx,by,gw,gh,Color(255,127,135,146));
  const int gx=bx+Pad,gy=by+Pad;
  for(int slot=0;slot<slots();++slot){
   int col=slot%cols(),row=slot/cols();
   const int x=gx+col*(CellW+Gap),y=gy+row*(CellH+Gap);
   box(g,float(x),float(y),CellW,CellH,Color(255,37,42,50));
   if(model_.selected==1){
    if(slot==1){ // MO's second info tile, no pseudo-cameo.
     const auto&u=model_.single;
     const int hp=std::max(0,u.health),maxHP=std::max(1,u.strength);
     label(g,L"HP "+std::to_wstring(hp),float(x+4),float(y+2),10,Color(255,238,246,247),true);
     box(g,float(x+4),float(y+20),52,5,Color(255,73,74,80));
     box(g,float(x+4),float(y+20),52*std::clamp(float(hp)/maxHP,0.f,1.f),5,
         hp<maxHP/3?Color(255,230,81,72):Color(255,95,203,105));
     label(g,u.level>=2?L"精英":u.level?L"老兵":L"普通",float(x+3),float(y+29),10,
           Color(255,231,211,126));
    }else{
     const auto&u=slot==0?model_.single:model_.passenger[page_*7+slot-2];
     cameo(g,u.type,u.kind,x,y,u.name);
    }
   }else{
    const auto&u=model_.groups[page_*9+slot];
    cameo(g,u.type,u.kind,x,y,u.name);
    if(u.count>1){
     const std::wstring n=L"x"+std::to_wstring(u.count);
     const int badgeW=std::min(41,12+int(n.size())*7);
     box(g,float(x+CellW-badgeW),float(y+CellH-17),float(badgeW),16,Color(235,12,12,15));
     label(g,n,float(x+CellW-badgeW+3),float(y+CellH-17),12,Color(255,255,241,187),true);
    }
   }
   outline(g,x,y,CellW,CellH,slot==focused_?Color(255,242,220,115):Color(255,89,97,107));
  }
  int footer=by+gh+2;
  if(details_){
   box(g,float(bx),float(footer),float(gw),39,Color(255,22,27,36));
   outline(g,bx,footer,gw,39,Color(255,89,100,113));
   if(model_.selected==1){
    const auto&u=model_.single;
    label(g,name(u.name).substr(0,16),float(bx+5),float(footer+1),11,Color(255,225,233,245),true);
    std::wstring more=L"HP "+std::to_wstring(std::max(0,u.health))+L"/"+std::to_wstring(std::max(0,u.strength));
    if(u.ammoMax>0)more+=L" 弹 "+std::to_wstring(std::max(0,u.ammo))+L"/"+std::to_wstring(u.ammoMax);
    else if(u.passengers>0)more+=L" 载员 "+std::to_wstring(u.passengers);
    label(g,more.substr(0,27),float(bx+5),float(footer+20),10,Color(255,187,206,224));
   }else if(focused_>=0&&focused_<slots()){
    const auto&u=model_.groups[page_*9+focused_];
    label(g,name(u.name).substr(0,19),float(bx+5),float(footer+2),11,Color(255,238,243,248),true);
    label(g,L"×"+std::to_wstring(u.count),float(bx+5),float(footer+20),10,Color(255,193,215,227));
   }else{
    label(g,std::to_wstring(model_.selected)+L" / "+std::to_wstring(model_.groupCount),float(bx+5),float(footer+8),11,Color(255,216,231,236));
   }
   footer+=42;
  }
  if(pages()>1){
   box(g,float(bx),float(footer),float(gw),20,Color(255,26,30,39));
   label(g,L"◀",float(bx+5),float(footer+1),12,Color(255,230,220,143),true);
   label(g,std::to_wstring(page_+1)+L"/"+std::to_wstring(pages()),
         float(bx+gw/2-9),float(footer+3),10,Color(255,239,242,245));
   label(g,L"▶",float(bx+gw-22),float(footer+1),12,Color(255,230,220,143),true);
  }
 }
 void click(int x,int y,bool right=false){
  if(x<ButtonW+3){
   // Bottom button folds; upper 'i' toggles details. Right-click fold
   // mirrors the reference panel's details gesture.
   const int foldY=height_-ButtonW-1;
   if(right||(!folded_&&y<foldY-2))details_=!details_;
   else{folded_=!folded_;page_=0;}
   InvalidateRect(window_,nullptr,FALSE);return;
  }
  if(folded_)return;
  int bx=ButtonW+4,by=Pad-2;
  const int gh=Pad*2+rows()*CellH+(rows()-1)*Gap;
  int footer=by+gh+(details_?44:2);
  if(pages()>1&&y>=footer){
   if(x<bx+40)page_=(page_+pages()-1)%pages();
   else if(x>bx+Pad*2+cols()*CellW+(cols()-1)*Gap-42)page_=(page_+1)%pages();
   focused_=-1;
  }else{
   const int gx=bx+Pad,gy=by+Pad;
   if(x>=gx&&y>=gy){
    int c=(x-gx)/(CellW+Gap),row=(y-gy)/(CellH+Gap);
    int cell=row*cols()+c;
    if(c<cols()&&cell>=0&&cell<slots()&&
       (x-gx)%(CellW+Gap)<CellW&&(y-gy)%(CellH+Gap)<CellH)
     focused_=cell; // visual focus only: no game selection or cargo command
   }
  }
  InvalidateRect(window_,nullptr,FALSE);
 }
 void wheel(int delta){
  if(folded_||pages()<2)return;
  page_=(page_+(delta>0?pages()-1:1))%pages();
  InvalidateRect(window_,nullptr,FALSE);
 }
 static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
  auto*self=reinterpret_cast<Panel*>(GetWindowLongPtrW(w,GWLP_USERDATA));
  if(msg==WM_NCCREATE){
   auto*cs=reinterpret_cast<CREATESTRUCTW*>(lp);
   SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
   return TRUE;
  }
  if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
  if(msg==WM_ERASEBKGND)return TRUE;
  if(msg==WM_LBUTTONDOWN){if(self)self->click(short(LOWORD(lp)),short(HIWORD(lp)));return 0;}
  if(msg==WM_RBUTTONDOWN){if(self)self->click(short(LOWORD(lp)),short(HIWORD(lp)),true);return 0;}
  if(msg==WM_MOUSEWHEEL){if(self)self->wheel(GET_WHEEL_DELTA_WPARAM(wp));return 0;}
  if(msg==WM_PAINT){
   PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);
   if(self&&dc){
    RECT r{};GetClientRect(w,&r);int cw=r.right,ch=r.bottom;
    if(cw>0&&ch>0){
     HDC mem=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
     info.bmiHeader.biWidth=cw;info.bmiHeader.biHeight=-ch;info.bmiHeader.biPlanes=1;
     info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
     void*bits=nullptr;HBITMAP image=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
     if(mem&&image){HGDIOBJ old=SelectObject(mem,image);
      {Gdiplus::Graphics g(mem);self->paint(g);}
      BitBlt(dc,0,0,cw,ch,mem,0,0,SRCCOPY);SelectObject(mem,old);
     }
     if(image)DeleteObject(image);if(mem)DeleteDC(mem);
    }
   }
   EndPaint(w,&ps);return 0;
  }
  return DefWindowProcW(w,msg,wp,lp);
 }
 bool ensure(){
  if(window_)return true;
  WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;
  wc.lpszClassName=L"RA2ToolboxUnitInfoPanel01";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
  if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
  window_=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
   wc.lpszClassName,L"",WS_POPUP,0,0,width_,height_,nullptr,nullptr,wc.hInstance,this);
  if(!window_)return false;
  SetLayeredWindowAttributes(window_,0,242,LWA_ALPHA);
  return true;
 }
public:
 template<class Read,class Resolve>void refreshCameos(
        const external::unitpanel::Model&model,Read read,Resolve resolve,
        um::AssetLoad files,uint32_t pid,uint32_t match,DWORD now){
  const auto before=cameos_.revision();
  cameos_.refresh(model,read,resolve,files,pid,match,now);
  if(cameos_.revision()!=before&&window_)InvalidateRect(window_,nullptr,FALSE);
 }
 void hide(){lastGoodAt_=0;if(window_&&IsWindowVisible(window_))ShowWindow(window_,SW_HIDE);}
 void miss(){if(lastGoodAt_&&DWORD(GetTickCount()-lastGoodAt_)<550)return;hide();}
 void destroy(){if(window_){DestroyWindow(window_);window_=nullptr;}cameos_.clear();model_={};signature_=0;}
 void update(const external::unitpanel::Model&model,int x,int bottom,int availableWidth){
  if(model.selected<=0||model.groupCount<=0||availableWidth<150){miss();return;}
  if(!ensure())return;
  lastGoodAt_=GetTickCount();
  const uint64_t key=model.visualSignature();
  bool newEpoch=model_.match!=model.match;
  if(newEpoch){page_=0;focused_=-1;}
  if(key!=signature_){model_=model;signature_=key;InvalidateRect(window_,nullptr,FALSE);}
  page_=external::unitpanel::clampPage(page_,entries(),perPage());
  int newWidth=std::min(wantedWidth(),availableWidth);
  int newHeight=wantedHeight();
  if(newWidth!=width_||newHeight!=height_){width_=newWidth;height_=newHeight;InvalidateRect(window_,nullptr,FALSE);}
  // Preserve game focus and never turn the panel into a gameplay click.
  RECT last{};const bool visible=IsWindowVisible(window_)!=FALSE;
  const bool same=visible&&GetWindowRect(window_,&last)&&last.left==x&&
    last.top==bottom-height_&&last.right-last.left==width_&&last.bottom-last.top==height_;
  if(!same)overlaystack::place(window_,overlaystack::Layer::UnitPanel,
    x,bottom-height_,width_,height_,SWP_NOACTIVATE|SWP_SHOWWINDOW);
 }
};
inline Panel panel;
} // namespace unitpanelhost
