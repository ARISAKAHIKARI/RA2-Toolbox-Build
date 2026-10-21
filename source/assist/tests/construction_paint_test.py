"""Execute shipped WM_PAINT/geometry with a GDI model, not a real compositor.

Every visible write is observed. Background/cards/text must remain offscreen
until a complete frame is copied. The previous host fails this assertion.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
host = Path(os.environ.get('CONSTRUCTION_HOST', root / 'assist/source/construction_host.h')).read_text()
position = host[host.index('static void resizePanel()'):host.index('static bool usable();')]
rect = host[host.index('static RECT rect('):host.index('static void paintIcon(')]
panel_box = host[host.index('static void panelBox('):host.index('static LRESULT CALLBACK proc(')]
proc = host[host.index('static LRESULT CALLBACK proc('):host.index('static void service()')]
prefix = r'''
#include "construction_policy.h"
#include <cassert>
#include <string>
#include <iostream>
#include <cstdint>
using namespace assist;
using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
#define CALLBACK
struct RECT{int left=0,top=0,right=0,bottom=0;};struct POINT{int x=0,y=0;};
struct Window{RECT bounds{};bool visible=false;};using HWND=Window*;
struct Object{int kind=0,w=0,h=0;uint32_t color=0;std::vector<uint32_t> pixels;};
using COLORREF=uint32_t;using HPEN=Object*;using HGDIOBJ=Object*;using HBITMAP=Object*;using HBRUSH=Object*;using HFONT=Object*;
static Object stockBitmap{1},stockFont{2},testFont{2},stockBrush{3},stockPen{4};
struct DC{HBITMAP bitmap=&stockBitmap;HFONT font=&stockFont;HBRUSH brush=&stockBrush;HPEN pen=&stockPen;POINT cursor{};uint32_t text=0;};using HDC=DC*;
static DC screen;struct PAINTSTRUCT{};
static int fail=0,dcLive=0,bitmapLive=0,allocations=0,blits=0,positions=0,paintStarts=0,paintEnds=0,lastFlags=0;
static bool originReady=true;static POINT origin{10,20};
static HGDIOBJ HGDI_ERROR=reinterpret_cast<HGDIOBJ>(intptr_t(-1));
constexpr int WM_MOUSEACTIVATE=1,WM_ERASEBKGND=2,WM_MOUSEWHEEL=3,WM_LBUTTONUP=4,WM_RBUTTONUP=5,WM_PAINT=6,WM_MOUSEMOVE=7,WM_MOUSELEAVE=8;
constexpr int PS_SOLID=0,TME_LEAVE=2;struct TRACKMOUSEEVENT{size_t cbSize;unsigned flags;HWND hwnd;unsigned hover;};static void TrackMouseEvent(TRACKMOUSEEVENT*){}
constexpr int MA_NOACTIVATE=3,FALSE=0,MK_SHIFT=4,TRANSPARENT=1,DT_CENTER=1,DT_VCENTER=2,DT_SINGLELINE=4,DT_WORDBREAK=8,DT_END_ELLIPSIS=16;
constexpr int SRCCOPY=1,SWP_NOACTIVATE=1,SWP_SHOWWINDOW=2,SWP_NOOWNERZORDER=4,SWP_NOCOPYBITS=8,SWP_NOZORDER=16;
static HWND HWND_TOPMOST=nullptr;
static uint32_t RGB(unsigned r,unsigned g,unsigned b){return r|(g<<8)|(b<<16);}
static unsigned HIWORD(uintptr_t n){return (n>>16)&65535;}static unsigned LOWORD(uintptr_t n){return n&65535;}
static HDC CreateCompatibleDC(HDC){if(fail==1)return nullptr;++dcLive;return new DC;}
static HBITMAP CreateCompatibleBitmap(HDC,int w,int h){if(fail==2)return nullptr;++bitmapLive;++allocations;auto p=new Object{1,w,h};p->pixels.resize(w*h,0xDEADBEEF);return p;}
static HGDIOBJ SelectObject(HDC dc,HGDIOBJ object){
 if(fail==3&&object->kind==1&&object!=&stockBitmap)return HGDI_ERROR;
 if(object->kind==1){auto old=dc->bitmap;dc->bitmap=object;return old;}
 if(object->kind==3){auto old=dc->brush;dc->brush=object;return old;}if(object->kind==4){auto old=dc->pen;dc->pen=object;return old;}
 assert(object->kind==2);auto old=dc->font;dc->font=object;return old;
}
static bool DeleteObject(HGDIOBJ p){assert(p!=&stockBitmap&&p!=&stockFont&&p!=&testFont);if(p->kind==1)--bitmapLive;delete p;return true;}
static bool DeleteDC(HDC dc){assert(dc->bitmap==&stockBitmap);--dcLive;delete dc;return true;}
static HBRUSH CreateSolidBrush(uint32_t c){auto p=new Object{3};p->color=c;return p;}
static void fill(HDC dc,const RECT&r,uint32_t c){
 assert(dc!=&screen && "partial background/card/text reached the visible window");
 auto p=dc->bitmap;for(int y=std::max(0,r.top);y<std::min(p->h,r.bottom);++y)for(int x=std::max(0,r.left);x<std::min(p->w,r.right);++x)p->pixels[y*p->w+x]=c;
}
static void FillRect(HDC dc,const RECT*r,HBRUSH b){fill(dc,*r,b->color);}
static HPEN CreatePen(int,int,uint32_t c){auto p=new Object{4};p->color=c;return p;}
static void RoundRect(HDC dc,int x,int y,int r,int b,int,int){fill(dc,{x,y,r,b},dc->brush->color);}
static void MoveToEx(HDC dc,int x,int y,void*){dc->cursor={x,y};}
static void LineTo(HDC dc,int x,int y){fill(dc,{std::min(x,dc->cursor.x),std::min(y,dc->cursor.y),std::max(x,dc->cursor.x)+1,std::max(y,dc->cursor.y)+1},dc->pen->color);dc->cursor={x,y};}
static void SetBkMode(HDC,int){}static void SetTextColor(HDC dc,uint32_t c){dc->text=c;}
static void DrawTextW(HDC dc,const wchar_t*t,int,RECT*r,int){if(*t)fill(dc,{r->left+1,r->top+1,r->right-1,std::min(r->bottom,r->top+3)},dc->text);}
static bool BitBlt(HDC to,int,int,int w,int h,HDC from,int,int,int){
 assert(to==&screen&&from!=&screen&&from->bitmap->w==w&&from->bitmap->h==h);
 assert(from->font==&stockFont);assert(std::find(from->bitmap->pixels.begin(),from->bitmap->pixels.end(),0xDEADBEEF)==from->bitmap->pixels.end());
 to->bitmap->w=w;to->bitmap->h=h;to->bitmap->pixels=from->bitmap->pixels;++blits;return true;
}
static HDC BeginPaint(HWND,PAINTSTRUCT*){++paintStarts;return &screen;}static void EndPaint(HWND,PAINTSTRUCT*){++paintEnds;}
static bool GetClientRect(HWND w,RECT*r){*r={0,0,w->bounds.right-w->bounds.left,w->bounds.bottom-w->bounds.top};return true;}
static bool ClientToScreen(HWND,POINT*p){*p=origin;return originReady;}
static bool IsWindowVisible(HWND w){return w->visible;}static bool GetWindowRect(HWND w,RECT*r){*r=w->bounds;return true;}
static void SetWindowPos(HWND w,HWND,int x,int y,int width,int height,int flags){++positions;lastFlags=flags;w->bounds={x,y,x+width,y+height};if(flags&SWP_SHOWWINDOW)w->visible=true;}
static void InvalidateRect(HWND,void*,int){}static LRESULT DefWindowProcW(HWND,UINT,WPARAM,LPARAM){return 0;}
#include "construction_buffer.h"
static Window gameObject,panelObject;static HWND game=&gameObject,window=&panelObject;
static construction::Layout layout;static construction::PanelBuffer panelBuffer;
static bool geometryReady=true;static int activeTab=0,selectedTab=-1,scroll=0,hoverChoice=-1,hoverQueue=-1;static HFONT panelFont=&testFont,font=&testFont;
static construction::Book books[4];struct Choice{int slot,index,tab;uint32_t type;bool future;};static std::vector<Choice> choices;
static struct{int typeCount=8;struct{int kind=7,index=0;wchar_t name[40]=L"Building";}types[8];}snap;
static int encode(int kind,int index){return kind==7?index:(kind<<16)|index;}static void refreshChoices(){}
static bool usable(){return true;}static void removeTop(bool){}static void removeItem(int,int,bool){}static void reserve(int){}
'''
icon = r'''
static void paintIcon(HDC dc,int index,int x,int y,bool gray){fill(dc,rect(x,y,x+60,y+48),gray?0x808080:0xF00000+index);}
'''
suffix = r'''
int main(){
 Object visible{1};screen.bitmap=&visible;
 for(int n=0;n<8;++n)snap.types[n].index=n;
 assert(layout.set(2560,1440,3840,2160));layout.content(5,8);books[0].items={0,1,2,3,4};
 for(int i=0;i<8;++i)choices.push_back({i,i,0,0,i>4});resizePanel();assert(positions==1&&panelObject.visible);
 // 10 Hz refreshes retain both position and GDI resources. No partial frame
 // is visible even while each background, icon and title is being drawn.
 for(int i=0;i<1000;++i){resizePanel();proc(window,WM_PAINT,0,0);}
 assert((lastFlags&SWP_NOACTIVATE)&&(lastFlags&SWP_NOOWNERZORDER));
 assert(positions==1&&allocations==1&&dcLive==1&&bitmapLive==1&&blits==1000);
 const auto complete=visible.pixels;proc(window,WM_PAINT,0,0);assert(visible.pixels==complete);
 books[0].stage=1;proc(window,WM_PAINT,0,0); // pending state uses same backing store
 assert(allocations==1);
 origin.x+=40;resizePanel();assert(positions==2&&(lastFlags&SWP_NOZORDER));
 int before=positions;originReady=false;resizePanel();assert(positions==before);originReady=true;
 // Hidden/not-ready states never erase the previous complete frame. Reopen
 // restores topmost once. Invalid tabs cannot index outside the queue books.
 geometryReady=false;before=blits;proc(window,WM_PAINT,0,0);resizePanel();assert(blits==before);
 geometryReady=true;activeTab=-1;proc(window,WM_PAINT,0,0);assert(blits==before);activeTab=0;
 panelObject.visible=false;resizePanel();assert(positions==3&&(lastFlags&SWP_SHOWWINDOW));
 // Content and DPI/viewport changes must reallocate once, cover every pixel,
 // and continue sharing Layout's real client transform.
 for(auto dims:std::vector<std::vector<int>>{{2560,1440,3840,2160},{2560,1440,2560,1440},{640,480,640,480}})
 for(int pct:{50,100,150,250})for(int q:{0,1,5}){
  layout.set(dims[0],dims[1],dims[2],dims[3],pct);layout.content(q,8);books[0].items.resize(q,1);resizePanel();proc(window,WM_PAINT,0,0);
  assert(visible.w==layout.width()&&visible.h==layout.height()&&dcLive==1&&bitmapLive==1);
 }
 // Allocation/selection failures cannot clear the screen or destroy the
 // previous usable buffer. Recovery retries at the next ordinary paint.
 for(int mode:{1,2,3}){
  before=blits;auto old=visible.pixels;panelObject.bounds.bottom+=5;fail=mode;proc(window,WM_PAINT,0,0);
  assert(blits==before&&visible.pixels==old&&dcLive==1&&bitmapLive==1);fail=0;proc(window,WM_PAINT,0,0);assert(blits==before+1);
 }
 assert(proc(window,WM_ERASEBKGND,0,0)==1);assert(paintStarts==paintEnds);
 panelBuffer.clear();assert(dcLive==0&&bitmapLive==0);panelBuffer.clear();
 std::cout<<"PASS: 1000 unchanged refreshes, offscreen-only draws and one complete presentation per paint; no redundant position or allocation\n";
 std::cout<<"PASS: 36 viewport/scale/queue combinations; move/hide/reopen/invalid-tab; DC/bitmap/selection failures and resource cleanup\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'paint.cpp'
    exe = Path(tmp) / 'paint'
    cpp.write_text(prefix + position + rect + icon + panel_box + proc + suffix)
    subprocess.run(['g++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                    '-I' + str(root / 'assist/source'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
