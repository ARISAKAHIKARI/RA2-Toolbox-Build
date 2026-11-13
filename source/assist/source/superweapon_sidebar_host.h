#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "construction_buffer.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../shared/source/overlay_stack.h"
#include "../../shared/source/overlay_visibility.h"
#include "../../external_reader/source/display_state.h"

// A detached, non-activating interaction panel. All game access is read-only;
// selection/cancellation returns through the existing authenticated command
// mailbox. The native support page remains intact, including during read loss.
#include "superweapon_sidebar_model.h"
#include "panel_placement.h"
namespace swsidebar {

class Panel {
 struct Icon {uint32_t shape=0;DWORD tried=0;external::voxel::Image image;};
 HWND window_=nullptr,game_=nullptr;uint32_t pid_=0,match_=0;bool inputReady_=false,shown_=false,drag_=false,buttonDown_=false;
 DWORD sampled_=0;int width_=0,height_=0,clientW_=0,clientH_=0,mapRight_=0,screenW_=0,logicalMapRight_=0,scroll_=0,hover_=-1,pressed_=-1;
 int left_=0,top_=0,pressX_=0,pressY_=0;POINT dragOffset_{};
 nativeviewportprojection::Transform viewport_{};assist::panelplacement::Rect occupied_{};
 Options options_;Read read_;std::function<void(int,uint32_t,uint32_t)>select_;std::function<void(const Options&)>changed_;
 Entry pressedEntry_;std::array<uint16_t,256>palette_{};bool paletteReady_=false;
 std::vector<Entry> entries_;std::map<uint32_t,Icon>icons_;assist::construction::PanelBuffer buffer_;
 int px(int n)const{return std::max(1,n*options_.scale/100);}
 int title()const{return px(20);}int cellW()const{return px(66);}int cellH()const{return px(65);}
 int columns()const{return 2;}
 int rows()const{return assist::panelplacement::rows(int(entries_.size()),options_.rows,(clientH_-title()-px(8))/cellH());}
 int hit(int x,int y)const{
  if(options_.collapsed||x<px(4)||y<title()+px(4))return -1;
  int col=(x-px(4))/cellW(),row=(y-title()-px(4))/cellH();if(col>=columns()||row>=rows())return -1;
  int n=scroll_+row*columns()+col;return n<int(entries_.size())?n:-1;
 }
 bool usable()const{return overlayvisibility::visible()&&inputReady_&&shown_&&game_&&GetForegroundWindow()==game_&&DWORD(GetTickCount()-sampled_)<250&&external::display::inspect(read_).visible();}
 void changed(){if(changed_)changed_(options_);}
 void geometry(){
  width_=columns()*cellW()+px(8);height_=title()+(options_.collapsed?0:rows()*cellH()+px(8));
  if(options_.detached){left_=std::clamp(options_.x,0,std::max(0,clientW_-width_));top_=std::clamp(options_.y,0,std::max(0,clientH_-height_));}
  else{left_=std::clamp(mapRight_-width_-px(4),0,std::max(0,clientW_-width_));top_=std::clamp((clientH_-height_)/2,0,std::max(0,clientH_-height_));}
  if(!drag_){auto r=assist::panelplacement::avoid({left_,top_,width_,height_},occupied_,clientW_,clientH_,px(4));left_=r.x;top_=r.y;}
  scroll_=std::clamp(scroll_,0,std::max(0,((int(entries_.size())+1)/2-rows())*2));
  POINT origin{};if(window_&&ClientToScreen(game_,&origin))overlaystack::place(window_,overlaystack::Layer::Construction,origin.x+left_,origin.y+top_,width_,height_,SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOCOPYBITS|(shown_?SWP_SHOWWINDOW:0));
 }
 void paint(HDC target){
  HDC dc=buffer_.acquire(target,width_,height_);if(!dc)return;
  Gdiplus::Graphics g(dc);g.Clear(Gdiplus::Color(255,35,39,34));
  Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255,62,67,53)),white(Gdiplus::Color(255,230,233,206)),muted(Gdiplus::Color(255,156,163,136));
  g.FillRectangle(&titleBrush,0,0,width_,title());
  Gdiplus::FontFamily family(L"Microsoft YaHei UI");Gdiplus::Font font(&family,float(px(11)),Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
  Gdiplus::StringFormat fmt;fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
  std::wstring text=hover_>=0&&hover_<int(entries_.size())?entries_[hover_].name:L"";g.DrawString(text.c_str(),-1,&font,Gdiplus::RectF(float(px(6)),float(px(4)),float(width_-px(48)),float(title())),&fmt,&white);
  g.DrawString(options_.detached?L"▣":L"↗",-1,&font,Gdiplus::PointF(float(width_-px(43)),float(px(4))),&white);
  g.DrawString(options_.collapsed?L"+":L"−",-1,&font,Gdiplus::PointF(float(width_-px(22)),float(px(4))),&white);
  if(!options_.collapsed)for(int slot=0,n=scroll_;slot<rows()*columns()&&n<int(entries_.size());++slot,++n){
   const auto&e=entries_[n];int x=px(4)+(slot%columns())*cellW(),y=title()+px(4)+(slot/columns())*cellH();
   Gdiplus::SolidBrush backing(Gdiplus::Color(255,n==hover_?55:28,n==hover_?64:33,n==hover_?43:26));g.FillRectangle(&backing,x,y,px(60),px(48));
   auto found=icons_.find(e.type);if(found!=icons_.end()&&!found->second.image.pixels.empty()){
    auto&im=found->second.image;Gdiplus::Bitmap bitmap(im.width,im.height,im.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(im.pixels.data()));
    float sx=float(px(60))/std::max(1,im.canvasWidth),sy=float(px(48))/std::max(1,im.canvasHeight);
    g.DrawImage(&bitmap,Gdiplus::RectF(float(x)+(im.x+im.canvasWidth/2)*sx,float(y)+(im.y+im.canvasHeight/2)*sy,im.width*sx,im.height*sy));
   }else g.DrawString(e.name,-1,&font,Gdiplus::RectF(float(x+px(3)),float(y+px(8)),float(px(54)),float(px(36))),&fmt,&muted);
   if(!e.ready){Gdiplus::SolidBrush dim(Gdiplus::Color(115,0,0,0));g.FillRectangle(&dim,x,y,px(60),px(48));}
   Gdiplus::Pen border(e.ready?Gdiplus::Color(255,181,193,137):Gdiplus::Color(255,91,102,76),float(px(1)));g.DrawRectangle(&border,x,y,px(60),px(48));
   Gdiplus::SolidBrush progress(Gdiplus::Color(255,139,158,99));g.FillRectangle(&progress,x,y+px(45),px(60)*e.percent/100,px(3));
   std::wstring state=e.suspended?L"Ⅱ":e.draining?L"▶":e.ready?L"":std::to_wstring((int64_t(e.frames)+14)/15)+L"s";
   g.DrawString(state.c_str(),-1,&font,Gdiplus::RectF(float(x),float(y+px(49)),float(px(62)),float(px(16))),&fmt,e.ready?&white:&muted);
  }
  if(hover_>=0&&hover_<int(entries_.size())){std::wstring name=entries_[hover_].name;SetWindowTextW(window_,name.c_str());}
  g.Flush(Gdiplus::FlushIntentionSync);buffer_.present(target);
 }
 static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
  auto*self=reinterpret_cast<Panel*>(GetWindowLongPtrW(w,GWLP_USERDATA));
  if(m==WM_NCCREATE){self=static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(!self)return DefWindowProcW(w,m,a,b);int x=short(LOWORD(b)),y=short(HIWORD(b));
  if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(m==WM_ERASEBKGND)return 1;
  if(m==WM_LBUTTONDOWN){if(!self->usable())return 0;self->buttonDown_=true;self->pressed_=self->hit(x,y);self->pressX_=x;self->pressY_=y;
   self->pressedEntry_=self->pressed_>=0?self->entries_[self->pressed_]:Entry{};SetCapture(w);
   if(y<self->title()&&x<self->width_-self->px(48)){self->drag_=true;self->dragOffset_={x,y};SetCapture(w);}return 0;}
  if(m==WM_MOUSEMOVE){
   if(self->drag_&&!self->usable()){self->hide();return 0;}
   if(self->drag_){POINT p{};GetCursorPos(&p);ScreenToClient(self->game_,&p);self->options_.detached=true;self->options_.x=p.x-self->dragOffset_.x;self->options_.y=p.y-self->dragOffset_.y;self->geometry();}
   else{int hover=self->hit(x,y);if(hover!=self->hover_){self->hover_=hover;InvalidateRect(w,nullptr,FALSE);}TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,w,0};TrackMouseEvent(&track);}return 0;}
  if(m==WM_MOUSELEAVE){self->hover_=-1;InvalidateRect(w,nullptr,FALSE);return 0;}
  if(m==WM_CAPTURECHANGED){self->buttonDown_=false;self->pressed_=-1;if(self->drag_){self->drag_=false;self->changed();}return 0;}
  if(m==WM_CANCELMODE){self->buttonDown_=false;self->pressed_=-1;self->drag_=false;if(GetCapture()==w)ReleaseCapture();return 0;}
  if(m==WM_LBUTTONUP){
   const bool down=self->buttonDown_;const int pressed=self->pressed_;const Entry expected=self->pressedEntry_;
   self->buttonDown_=false;self->pressed_=-1;
   if(self->drag_){self->drag_=false;if(GetCapture()==w)ReleaseCapture();self->options_.x=self->left_;self->options_.y=self->top_;self->geometry();self->changed();return 0;}
   if(GetCapture()==w)ReleaseCapture();if(!down)return 0;
   if(!self->usable())return 0;
   if(y<self->title()&&self->pressY_<self->title()){
    if(x>=self->width_-self->px(24)){self->options_.collapsed=!self->options_.collapsed;self->geometry();self->changed();InvalidateRect(w,nullptr,FALSE);}
    else if(x>=self->width_-self->px(48)){self->options_.detached=!self->options_.detached;self->options_.x=self->left_;self->options_.y=self->top_;self->geometry();self->changed();InvalidateRect(w,nullptr,FALSE);}return 0;
   }
   int n=self->hit(x,y);if(n>=0&&n==pressed&&sameSelection(expected,self->entries_[n])&&self->entries_[n].ready&&self->select_&&selectable(self->read_,expected))self->select_(expected.index,expected.instance,expected.type);return 0;
  }
  if(m==WM_RBUTTONUP){if(self->usable()&&self->select_)self->select_(-1,0,0);return 0;}
  if(m==WM_MOUSEWHEEL){if(!self->usable())return 0;self->scroll_+=short(HIWORD(a))<0?2:-2;self->geometry();InvalidateRect(w,nullptr,FALSE);return 0;}
  if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);self->paint(dc);EndPaint(w,&ps);return 0;}
  return DefWindowProcW(w,m,a,b);
 }
 void loadOneIcon(){
  std::array<uint32_t,256>colors{};uint32_t converter=0,palette=0;int mode=0;uint16_t native[256]{};
  if(!get(read_,0x87F6B0,converter)||!get(read_,converter+4,mode)||mode!=2||!get(read_,converter+0x174,palette)||!bytes(read_,palette,native,sizeof native))return;
  uint16_t after[256]{};uint32_t verified=0;
  if(!get(read_,0x87F6B0,verified)||verified!=converter||!get(read_,converter+0x174,verified)||verified!=palette||!bytes(read_,palette,after,sizeof after)||std::memcmp(native,after,sizeof native))return;
  if(!paletteReady_||std::memcmp(palette_.data(),native,sizeof native)){
   std::memcpy(palette_.data(),native,sizeof native);paletteReady_=true;icons_.clear();InvalidateRect(window_,nullptr,FALSE);
  }
  for(int i=1;i<256;++i){unsigned c=native[i];colors[i]=0xFF000000u|((((c>>11)&31)*255/31)<<16)|((((c>>5)&63)*255/63)<<8)|((c&31)*255/31);}
  DWORD now=GetTickCount();for(auto&e:entries_){auto&icon=icons_[e.type];if(icon.shape!=e.shape){icon.image={};icon.tried=0;icon.shape=e.shape;InvalidateRect(window_,nullptr,FALSE);}
   if(!icon.image.pixels.empty())continue;if(icon.tried&&DWORD(now-icon.tried)<1000)continue;
   icon.tried=now;external::voxel::Image next;
   if(e.shape>=0x10000&&external::voxel::sprite(read_,e.shape,colors,next,0,nullptr,255)){icon.image=std::move(next);InvalidateRect(window_,nullptr,FALSE);}break;
  }
 }
public:
 Panel()=default;Panel(const Panel&)=delete;Panel&operator=(const Panel&)=delete;~Panel(){destroy();}
 void hide(){inputReady_=shown_=buttonDown_=false;pressed_=hover_=-1;drag_=false;if(window_&&GetCapture()==window_)ReleaseCapture();if(window_)ShowWindow(window_,SW_HIDE);}
 void destroy(){hide();if(window_){overlaystack::forget(window_);DestroyWindow(window_);window_=nullptr;}buffer_.clear();icons_.clear();entries_.clear();sampled_=0;}
 // physicalMapRight is the physical viewport A right edge in game screen
 // pixels (before scaling to the HWND client), not the enlarged zoom-out R.
 // It is transient geometry and must not be saved in Options.
 void update(HWND game,uint32_t process,uint32_t match,bool inputReady,const Options&options,Read read,
             std::function<void(int,uint32_t,uint32_t)>select,std::function<void(const Options&)>changed,int physicalMapRight=0,const nativeviewportprojection::Transform&transform={},assist::panelplacement::Rect occupied={}){
  occupied_=occupied;
  if(viewport_!=transform){hide();sampled_=0;viewport_=transform;}
  if(!drag_)options_=options;options_.scale=std::clamp(options_.scale,75,200);options_.rows=std::clamp(options_.rows,1,20);read_=std::move(read);select_=std::move(select);changed_=std::move(changed);
  if(!overlayvisibility::visible()||!options.enabled||!game||!match||!IsWindow(game)||IsIconic(game)||GetForegroundWindow()!=game){hide();return;}
  if(pid_!=process||match_!=match||game_!=game){hide();pid_=process;match_=match;game_=game;icons_.clear();paletteReady_=false;entries_.clear();sampled_=0;scroll_=0;}
  RECT client{};if(!GetClientRect(game_,&client)||client.right<100||client.bottom<100){hide();return;}clientW_=client.right;clientH_=client.bottom;
  if(!window_){WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2ToolboxDetachedSuperweapon139";wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));RegisterClassW(&wc);
   window_=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,L"超级武器",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,this);if(!window_)return;}
  const auto display=external::display::inspect(read_);if(display.readable&&!display.visible()){hide();return;}
  inputReady=inputReady&&display.visible();
  DWORD now=GetTickCount();if(!sampled_||DWORD(now-sampled_)>=100){std::vector<Entry> next;int w=0,h=0,right=0;
   if(!sample(read_,next,w,h,right,transform)){inputReady_=false;if(!sampled_||DWORD(now-sampled_)>=250)hide();return;}
   bool changedEntries=next!=entries_;entries_=std::move(next);sampled_=now;screenW_=w;logicalMapRight_=right;
   for(auto it=icons_.begin();it!=icons_.end();)if(std::none_of(entries_.begin(),entries_.end(),[&](const Entry&e){return e.type==it->first;}))it=icons_.erase(it);else ++it;
   if(changedEntries)InvalidateRect(window_,nullptr,FALSE);
  }
  mapRight_=int(int64_t(std::clamp(physicalMapRight>0?physicalMapRight:logicalMapRight_,1,std::max(1,screenW_)))*clientW_/std::max(1,screenW_));
  inputReady_=inputReady;shown_=!entries_.empty();if(!shown_){hide();return;}geometry();loadOneIcon();
 }
 const Options&options()const{return options_;}
};
}
