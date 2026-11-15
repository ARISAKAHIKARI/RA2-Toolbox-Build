#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <algorithm>
#include <cstdint>
#include "../../external_reader/source/unit_info_panel_model.h"
#include "../../shared/source/overlay_stack.h"

// RA2 Toolbox's original unit panel implementation. No MO source/assets,
// engine hooks, game events or game-memory reads live in this UI object.
// Only page/collapse/detail interactions are accepted in v1. The popup HWND
// consumes panel clicks, unlike a WS_EX_TRANSPARENT world overlay.
namespace unitpanelhost {
class Panel {
 HWND window_=nullptr;
 external::unitpanel::Model model_{};
 uint64_t signature_=0;
 bool folded_=false,details_=true;
 int page_=0,focused_=-1,width_=390,height_=40;
 static constexpr int Header=38,Footer=30;
 static std::wstring name(const std::array<uint16_t,40>&src){
  std::wstring value;for(uint16_t c:src){if(!c)break;if(c>=32)value.push_back(wchar_t(c));}
  return value.empty()?L"未知单位":value;
 }
 int entries()const{return model_.selected==1?model_.passengerCount:model_.groupCount;}
 int perPage()const{return model_.selected==1?6:8;}
 int pages()const{return external::unitpanel::pageCount(entries(),perPage());}
 int rows()const{return (std::min(perPage(),std::max(0,entries()-page_*perPage()))+1)/2;}
 int wantedHeight()const{
  if(folded_)return Header;
  if(model_.selected==1){
   return Header+78+(model_.single.passengers>0?24+std::max(1,rows())*45:24)+Footer;
  }
  return Header+std::max(1,rows())*49+Footer+8;
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
 void paint(Gdiplus::Graphics&g){
  using namespace Gdiplus;
  g.SetSmoothingMode(SmoothingModeAntiAlias);
  g.Clear(Color(255,16,24,37));
  box(g,0,0,float(width_),4,Color(255,87,181,226));
  box(g,0,Header-1,float(width_),1,Color(255,58,73,92));
  const std::wstring heading=model_.selected==1?L"单位信息 / 载员":L"选中单位 · "+std::to_wstring(model_.selected);
  label(g,heading,12,10,14,Color(255,235,246,255),true);
  box(g,float(width_-38),4,34,30,Color(255,40,58,75));
  label(g,folded_?L"+":L"−",float(width_-27),9,16,Color(255,218,235,249),true);
  if(folded_)return;
  int y=Header+5;
  if(model_.selected==1){
   const auto&u=model_.single;
   box(g,9,float(y),float(width_-18),72,Color(255,27,38,54));
   box(g,16,float(y+11),47,47,tint(u.kind));
   label(g,u.name.empty()?L"?":name(u.name).substr(0,1),29,float(y+18),22,Color(255,21,30,38),true);
   label(g,name(u.name),72,float(y+8),14,Color(255,246,250,254),true);
   label(g,L"生命 "+std::to_wstring(std::max(0,u.health))+L" / "+std::to_wstring(std::max(0,u.strength)),
     72,float(y+30),12,Color(255,196,214,230));
   float hpRatio=u.strength>0?std::clamp(float(u.health)/u.strength,0.f,1.f):0.f;
   box(g,72,float(y+51),float(width_-96),5,Color(255,64,68,77));
   box(g,72,float(y+51),float(width_-96)*hpRatio,5,
       hpRatio<=.25f?Color(255,238,91,91):hpRatio<=.5f?Color(255,241,185,81):Color(255,93,209,140));
   const std::wstring extras=L"等级 "+std::to_wstring(u.level)+
     (u.ammoMax>0?L"    弹药 "+std::to_wstring(std::max(0,u.ammo))+L"/"+std::to_wstring(u.ammoMax):L"");
   label(g,extras,72,float(y+58),10,Color(255,156,188,215));
   y+=79;
   if(u.passengers>0){
    label(g,L"车内载员 "+std::to_wstring(u.passengers)+(u.capacity>0?L" / "+std::to_wstring(u.capacity):L""),
          12,float(y),12,Color(255,190,210,226),true);
    y+=24;
    if(!model_.passengersComplete){
     label(g,L"载员链尚未完成验证",16,float(y+8),12,Color(255,255,194,118));
    }else{
     const int start=page_*perPage();
     for(int i=start;i<std::min(start+perPage(),model_.passengerCount);++i){
      const auto&p=model_.passenger[i];int col=(i-start)%2,row=(i-start)/2;
      float cx=10+col*((width_-20)/2.f),cy=y+row*45.f;
      box(g,cx,cy,float((width_-20)/2-4),41,Color(255,36,49,65));
      box(g,cx+5,cy+5,31,31,tint(p.kind));
      label(g,name(p.name).substr(0,1),cx+13,cy+8,15,Color(255,20,30,41),true);
      label(g,name(p.name).substr(0,9),cx+41,cy+4,11,Color(255,239,246,252),true);
      label(g,L"HP "+std::to_wstring(std::max(0,p.health)),cx+41,cy+23,10,Color(255,157,191,217));
     }
    }
    y+=std::max(1,rows())*45;
   }else{
    label(g,L"无载员 · 当前仅查看，不修改游戏命令",13,float(y+2),11,Color(255,148,178,200));
    y+=24;
   }
  }else{
   int start=page_*perPage();
   for(int i=start;i<std::min(start+perPage(),model_.groupCount);++i){
    const auto&u=model_.groups[i];int col=(i-start)%2,row=(i-start)/2;
    float cx=10+col*((width_-20)/2.f),cy=y+row*49.f;
    box(g,cx,cy,float((width_-20)/2-5),44,
        focused_==i?Color(255,62,79,98):Color(255,31,45,62));
    box(g,cx+6,cy+6,33,32,tint(u.kind));
    label(g,name(u.name).substr(0,1),cx+15,cy+10,15,Color(255,23,33,42),true);
    label(g,name(u.name).substr(0,9),cx+45,cy+5,11,Color(255,236,245,252),true);
    label(g,L"× "+std::to_wstring(u.count),cx+45,cy+24,11,Color(255,146,214,244),true);
   }
   y+=std::max(1,rows())*49+7;
  }
  box(g,0,float(height_-Footer),float(width_),1,Color(255,58,73,92));
  label(g,L"仅查看 · 不改变游戏选中",12,float(height_-Footer+8),10,Color(255,143,174,197));
  if(pages()>1){
   label(g,L"‹",float(width_-91),float(height_-Footer+5),18,Color(255,210,236,251),true);
   label(g,std::to_wstring(page_+1)+L"/"+std::to_wstring(pages()),
     float(width_-71),float(height_-Footer+9),10,Color(255,197,222,237));
   label(g,L"›",float(width_-22),float(height_-Footer+5),18,Color(255,210,236,251),true);
  }
 }
 void click(int x,int y,bool right=false){
  if(y<Header&&x>=width_-48){folded_=!folded_;page_=0;InvalidateRect(window_,nullptr,FALSE);return;}
  if(folded_){folded_=false;InvalidateRect(window_,nullptr,FALSE);return;}
  if(right){focused_=-1;InvalidateRect(window_,nullptr,FALSE);return;}
  if(y>=height_-Footer&&pages()>1){
   if(x>=width_-102&&x<width_-75)page_=(page_+pages()-1)%pages();
   else if(x>=width_-42)page_=(page_+1)%pages();
  }else if(model_.selected>1&&y>=Header){
   const int col=std::clamp((x-10)/std::max(1,(width_-20)/2),0,1);
   const int row=std::clamp((y-(Header+5))/49,0,3);
   const int idx=page_*perPage()+row*2+col;
   if(idx<model_.groupCount)focused_=idx; // local detail focus only
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
 void hide(){if(window_&&IsWindowVisible(window_))ShowWindow(window_,SW_HIDE);}
 void destroy(){if(window_){DestroyWindow(window_);window_=nullptr;}model_={};signature_=0;}
 void update(const external::unitpanel::Model&model,int x,int bottom,int availableWidth){
  if(model.selected<=0||model.groupCount<=0||availableWidth<230){hide();return;}
  if(!ensure())return;
  const uint64_t key=model.visualSignature();
  bool newEpoch=model_.match!=model.match;
  if(newEpoch){page_=0;focused_=-1;}
  if(key!=signature_){model_=model;signature_=key;InvalidateRect(window_,nullptr,FALSE);}
  page_=external::unitpanel::clampPage(page_,entries(),perPage());
  int newWidth=std::min(390,availableWidth);
  int newHeight=wantedHeight();
  if(newWidth!=width_||newHeight!=height_){width_=newWidth;height_=newHeight;InvalidateRect(window_,nullptr,FALSE);}
  // Preserve game focus and never turn the panel into a gameplay click.
  overlaystack::place(window_,overlaystack::Layer::UnitPanel,
    x,bottom-height_,width_,height_,SWP_NOACTIVATE|SWP_SHOWWINDOW);
 }
};
inline Panel panel;
} // namespace unitpanelhost
